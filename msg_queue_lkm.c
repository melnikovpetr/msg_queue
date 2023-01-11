#include "msg_queue.h"

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/sched.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Petr Melnikov");
MODULE_DESCRIPTION("A Linux loadable kernel module for the message queue");
MODULE_VERSION("0.1");

#define CLASS_NAME  "msg_queue_cls"

static int lkm_major_number;

static struct class*  lkm_class  = NULL;
static struct device* lkm_device = NULL;

static int     dev_open(struct inode*, struct file*);
static long    dev_ioctl(struct file*, unsigned int, unsigned long);
static ssize_t dev_read(struct file*, char*, size_t, loff_t*);
static ssize_t dev_write(struct file*, const char*, size_t, loff_t*);
static int     dev_release(struct inode*, struct file*);

static struct file_operations dev_oper =
{
	.open           = dev_open,
	.unlocked_ioctl = dev_ioctl,
    .read           = dev_read,
    .write          = dev_write,
    .release        = dev_release,
};

struct queue_elem_t;

static struct queue_elem_t* queue_crt(void);
static void queue_del(struct queue_elem_t* queue_elem);

static char* queue_msg(struct queue_elem_t* queue_elem);
static size_t queue_msg_size(struct queue_elem_t* queue_elem);
static void queue_set_msg_size(struct queue_elem_t* queue_elem, size_t size);
static struct queue_elem_t* queue_prev(struct queue_elem_t* queue_elem);

static void queue_ins(struct queue_elem_t* queue_elem, struct queue_elem_t* before_this);
static void queue_rmv(struct queue_elem_t* queue_elem);
static void queue_del_all(struct queue_elem_t* queue_elem);

static ssize_t queue_read_msg(struct file* fp, ssize_t (*read)(struct file*, char*, size_t, loff_t*), struct queue_elem_t* queue_elem);
static ssize_t queue_write_msg(struct file* fp, ssize_t (*write)(struct file*, const char*, size_t, loff_t*), struct queue_elem_t* queue_elem);

static ssize_t queue_read(struct file* fp, ssize_t (*read)(struct file*, char*, size_t, loff_t*), size_t max_size, struct queue_elem_t** first, struct queue_elem_t** last);
static ssize_t queue_write(struct file* fp, ssize_t (*write)(struct file*, const char*, size_t, loff_t*), struct queue_elem_t* pos);

static struct file* file_open(const char* path, int flags, int rights);
static void file_close(struct file* fp);
static ssize_t file_read(struct file* fp, char* buffer, size_t len, loff_t* off);
static ssize_t file_write(struct file* fp, const char* buffer, size_t len, loff_t* off);

struct queue_t
{
	struct queue_elem_t* first;
	struct queue_elem_t* last;
	size_t size;
};

static struct queue_t queue =
{
	.first = NULL,
	.last = NULL,
	.size = 0,
};

static spinlock_t queue_lock = __SPIN_LOCK_UNLOCKED();

static struct workqueue_struct* queue_works = NULL;

static wait_queue_head_t queue_waits;

struct queue_work_data_t
{
    struct work_struct work;
    unsigned int cmd;
	char* path;
};

static long queue_cmd(struct work_struct* work_data);

static void queue_work_fn(struct work_struct* work_data)
{
	queue_cmd(work_data);
}

static long queue_cmd(struct work_struct* work_data)
{
	long ret = 0;
	struct queue_work_data_t* queue_work_data = (struct queue_work_data_t*)work_data;
	unsigned int cmd = queue_work_data->cmd;
	char* path = queue_work_data->path;

	if (cmd == MSG_QUEUE_LOAD)
	{
		struct queue_elem_t* first = NULL;
		struct queue_elem_t* last = NULL;

		struct file* in_fp = file_open(path, O_RDONLY, 0);

		if (IS_ERR(in_fp))
		{
			printk(KERN_ALERT "msg_queue_lkm: failed to open input file [%s]\n", path);
			kfree(path);
			return PTR_ERR(in_fp);
		}

		printk(KERN_INFO "msg_queue_lkm: starting to load messages from the file [%s]\n", path);

		kfree(path);

		ret = queue_read(in_fp, file_read, MAX_QUEUE_SIZE, &first, &last);

		if (ret < 0) printk(KERN_ALERT "msg_queue_lkm: failed to read message queue from the file\n");
		else
		{
			if (ret > 0)
			{
				struct queue_elem_t* old_first = NULL;
				spin_lock(&queue_lock);
				{
					old_first = queue.first;
					queue.first = first;
					queue.last = last;
					queue.size = ret;
				}
				spin_unlock(&queue_lock);
				queue_del_all(old_first);
			}
			printk(KERN_INFO "msg_queue_lkm: %zd messages have been read from the file\n", ret);
			wake_up_interruptible(&queue_waits);
		}
		file_close(in_fp);
	} else
	if (cmd == MSG_QUEUE_SAVE)
	{
		struct queue_t old_queue = {0};

		struct file* out_fp = file_open(path, O_CREAT | O_WRONLY | O_APPEND, 0666);

		if (IS_ERR(out_fp))
		{
			printk(KERN_ALERT "msg_queue_lkm: failed to open output file [%s]\n", path);
			kfree(path);
			return PTR_ERR(out_fp);
		}

		printk(KERN_INFO "msg_queue_lkm: starting to write messages to the file [%s]\n", path);

		kfree(path);

		spin_lock(&queue_lock);
		{
			old_queue = queue;
			queue.first = NULL;
			queue.last = NULL;
			queue.size = 0;
		}
		spin_unlock(&queue_lock);

		ret = queue_write(out_fp, file_write, old_queue.last);

		if (ret < 0)
		{
			spin_lock(&queue_lock);
			{
				queue = old_queue;
			}
			spin_unlock(&queue_lock);
			printk(KERN_ALERT "msg_queue_lkm: failed to write message queue to the file\n");
		}
		else
		{
			queue_del_all(old_queue.first);
			printk(KERN_INFO "msg_queue_lkm: %zd message(s) have been written to the file\n", ret);
		}
		file_close(out_fp);
	}
	else
	{
		return -ENOTTY;
	}

	return ret;

	kfree(queue_work_data);
}

static int /*__init*/ lkm_init(void)
{
	printk(KERN_INFO "msg_queue_lkm: initializing the message queue LKM\n");

	lkm_major_number = register_chrdev(0, DEVICE_NAME, &dev_oper);
	if (lkm_major_number < 0)
	{
		printk(KERN_ALERT "msg_queue_lkm: message queue LKM failed to register a major number\n");
		return lkm_major_number;
	}
	printk(KERN_INFO "msg_queue_lkm: registered correctly with major number %d\n", lkm_major_number);

	lkm_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(lkm_class))
	{
		unregister_chrdev(lkm_major_number, DEVICE_NAME);
		printk(KERN_ALERT "msg_queue_lkm: failed to register device class\n");
		return PTR_ERR(lkm_class);
	}
	printk(KERN_INFO "msg_queue_lkm: device class registered correctly\n");

	lkm_device = device_create(lkm_class, NULL, MKDEV(lkm_major_number, 0), NULL, DEVICE_NAME);
	if (IS_ERR(lkm_device))
	{
		class_destroy(lkm_class);
		unregister_chrdev(lkm_major_number, DEVICE_NAME);
		printk(KERN_ALERT "msg_queue_lkm: failed to create the device\n");
		return PTR_ERR(lkm_device);
	}
	printk(KERN_INFO "msg_queue_lkm: device class created correctly\n");

	spin_lock_init(&queue_lock);

    queue_works = create_singlethread_workqueue(DEVICE_NAME);
    if (IS_ERR(queue_works))
    {
        printk(KERN_ALERT "msg_queue_lkm: failed to create the workqueue\n");
        return PTR_ERR(queue_works);
    }

    init_waitqueue_head(&queue_waits);

	return 0;
}

static void /*__exit*/ lkm_exit(void)
{
	struct queue_elem_t* first = NULL;

	spin_lock(&queue_lock);
	{
		first = queue.first;
		queue.first = NULL;
		queue.last = NULL;
		queue.size = 0;
	}
	spin_unlock(&queue_lock);

	queue_del_all(first);

	device_destroy(lkm_class, MKDEV(lkm_major_number, 0));
	class_unregister(lkm_class);
	class_destroy(lkm_class);
	unregister_chrdev(lkm_major_number, DEVICE_NAME);
    if (queue_works) destroy_workqueue(queue_works);
	printk(KERN_INFO "msg_queue_lkm: message queue LKM unloaded!\n");
}

static int dev_open(struct inode* ndp, struct file* fp)
{
	printk(KERN_INFO "msg_queue_lkm: device has been opened\n");
	return 0;
}

static long dev_ioctl(struct file* fp, unsigned int cmd, unsigned long args)
{
	size_t path_len = strnlen_user((const char __user*)args, PATH_MAX);
	struct queue_work_data_t* queue_work_data = NULL;

	queue_work_data = kmalloc(sizeof(struct queue_work_data_t), GFP_KERNEL);
	if (!queue_work_data) return -ENOMEM;

	queue_work_data->path = kmalloc(path_len + 1, GFP_KERNEL);
	if (!queue_work_data->path) return -ENOMEM;
	copy_from_user(queue_work_data->path, (const char __user*)args, path_len + 1);

    if (cmd == MSG_QUEUE_LOAD_ASYNC)
    {
		INIT_WORK(&queue_work_data->work, queue_work_fn);
		queue_work_data->cmd = MSG_QUEUE_LOAD;
        printk(KERN_INFO "msg_queue_lkm: starting async work");
        queue_work(queue_works, &queue_work_data->work);
    } else
    if (cmd == MSG_QUEUE_SAVE_ASYNC)
    {
		INIT_WORK(&queue_work_data->work, queue_work_fn);
		queue_work_data->cmd = MSG_QUEUE_SAVE;
        printk(KERN_INFO "msg_queue_lkm: starting async work");
        queue_work(queue_works, &queue_work_data->work);
    }
    else
    {
		queue_work_data->cmd = cmd;
		return queue_cmd(&queue_work_data->work);
    }
	return 0;
}

static ssize_t dev_read(struct file* fp, char* buffer, size_t len, loff_t* off)
{
	size_t queue_new_size = 0;
    struct queue_elem_t* last = NULL;

    do
    {
        spin_lock(&queue_lock);
        {
            if (queue.size > 0)
            {
                last = queue.last;
                queue.last = queue_prev(queue.last);
                if (!queue.last) queue.first = NULL;
                queue_rmv(last);
                queue_new_size = --queue.size;
            }
        }
        spin_unlock(&queue_lock);

        if (last != NULL)
        {
            size_t msg_size = min(len, queue_msg_size(last));
            size_t error_count = copy_to_user(buffer, queue_msg(last), msg_size);

            if (error_count != 0)
            {
                printk(KERN_ALERT "msg_queue_lkm: failed to send %zu characters to the user\n", error_count);
            }

            msg_size -= error_count;

            printk(KERN_INFO "msg_queue_lkm: the queue size was decremented (new size = %zu)\n", queue_new_size);
            return msg_size;
        }
    }
    while(!(fp->f_flags & O_NONBLOCK) && !wait_event_interruptible(queue_waits, queue.size));

    printk(KERN_INFO "msg_queue_lkm: the queue is empty\n");
    return -EEMPTY;
}

static ssize_t dev_write(struct file* fp, const char* buffer, size_t len, loff_t* off)
{
    size_t queue_new_size = 0;
    struct queue_elem_t* first = queue_crt();
	if (first)
	{
		size_t msg_size = min(len, (size_t)MAX_MSG_SIZE);
        size_t error_count = copy_from_user(queue_msg(first), buffer, msg_size);

        if (error_count != 0)
        {
           printk(KERN_ALERT "msg_queue_lkm: failed to send %zu characters from the user\n", error_count);
        }

        msg_size -= error_count;
        queue_set_msg_size(first, msg_size);

        spin_lock(&queue_lock);
		{
            if (queue.size < MAX_QUEUE_SIZE)
            {
                if (queue.first != NULL)
                {
                    queue_ins(first, queue.first);
                }
                else
                {
                    queue.last = first;
                }
                queue.first = first;
                queue_new_size = ++queue.size;
            }
		}
		spin_unlock(&queue_lock);

        if (!queue_new_size)
        {
            queue_del(first);
			printk(KERN_ALERT "msg_queue_lkm: failed to push message, the queue is full [size = %zu]\n", (size_t)MAX_QUEUE_SIZE);
            return -EFULL;
        }
        else
        {
            wake_up_interruptible(&queue_waits);
            printk(KERN_INFO "msg_queue_lkm: the queue size was incremented [size = %zu]\n", queue_new_size);
            return msg_size;
        }
	}
    else
    {
        printk(KERN_ALERT "msg_queue_lkm: failed to allocate memory for a new queue element\n");
        return -ENOMEM;
    }
}

static int dev_release(struct inode* ndp, struct file* fp)
{
   printk(KERN_INFO "msg_queue_lkm: device successfully closed\n");
   return 0;
}

module_init(lkm_init)
module_exit(lkm_exit)

#include "msg_queue_lkm_fops.c"
#include "msg_queue_lkm_qops.c"
