#include "msg_queue.h"

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>

struct queue_elem_t
{
	struct queue_elem_t* prev;
	struct queue_elem_t* next;
	char msg[MAX_MSG_SIZE];
	size_t size;
};

static struct queue_elem_t* queue_crt(void)
{
    struct queue_elem_t* queue_elem = kmalloc(sizeof(struct queue_elem_t), GFP_KERNEL);
    if (queue_elem)
    {
        queue_elem->prev = NULL;
        queue_elem->next = NULL;
        queue_elem->size = 0;
    }
    return queue_elem;
}

static void queue_del(struct queue_elem_t* queue_elem)
{
    kfree(queue_elem);
}

static char* queue_msg(struct queue_elem_t* queue_elem)
{
    if (queue_elem) return queue_elem->msg;
    return NULL;
}

static size_t queue_msg_size(struct queue_elem_t* queue_elem)
{
	if (queue_elem) return queue_elem->size;
	return 0;
}

static void queue_set_msg_size(struct queue_elem_t* queue_elem, size_t size)
{
    if (queue_elem) queue_elem->size = size;
}

static struct queue_elem_t* queue_prev(struct queue_elem_t* queue_elem)
{
	if (queue_elem) return queue_elem->prev;
	return NULL;
}

static void queue_ins(struct queue_elem_t* queue_elem, struct queue_elem_t* before_this)
{
    if (before_this)
    {
        before_this->prev = queue_elem;
        if (queue_elem) queue_elem->next = before_this;
    }
}

static void queue_rmv(struct queue_elem_t* queue_elem)
{
	if (queue_elem)
	{
		if (queue_elem->next) queue_elem->next->prev = queue_elem->prev;
		if (queue_elem->prev) queue_elem->prev->next = queue_elem->next;
	}
}

static void queue_del_all(struct queue_elem_t* pos)
{
    for (;pos != NULL;)
    {
        struct queue_elem_t* tmp = pos;
        pos = pos->next;
        queue_rmv(tmp);
        queue_del(tmp);
    }
}

static ssize_t queue_read_msg(struct file* fp, ssize_t (*read)(struct file*, char*, size_t, loff_t*), struct queue_elem_t* queue_elem)
{
	if (queue_elem)
	{
		ssize_t ret = read(fp, (char*)&queue_elem->size, sizeof(queue_elem->size), &fp->f_pos);
		printk(KERN_INFO "msg_queue_lkm: queue_read_msg size[%lu]", queue_elem->size);
		if ((ret != sizeof(queue_elem->size)) || (queue_elem->size > MAX_MSG_SIZE))
		{
			if (ret >= 0) ret = -EINVAL;
		}
		else
		{
			ret = read(fp, queue_elem->msg, queue_elem->size, &fp->f_pos);
			printk(KERN_INFO "msg_queue_lkm: queue_read_msg msg[%s]", queue_elem->msg);
			if ((ret > 0) && (queue_elem->size != (size_t)ret)) ret = -EINVAL;
		}
		return ret;
	}
	return -EFAULT;
}

static ssize_t queue_write_msg(struct file* fp, ssize_t (*write)(struct file*, const char*, size_t, loff_t*), struct queue_elem_t* queue_elem)
{
	if (queue_elem)
	{
		ssize_t ret = write(fp, (char*)&queue_elem->size, sizeof(queue_elem->size), &fp->f_pos);
		if (ret != sizeof(queue_elem->size))
		{
			if (ret >= 0) ret = -EFAULT ;
		}
		else
		{
			ret = write(fp, queue_elem->msg, queue_elem->size, &fp->f_pos);
			if ((ret > 0) && (queue_elem->size != (size_t)ret)) ret = -EINVAL;
		}
		return ret;
	}
	return -EFAULT;
}

static ssize_t queue_read(struct file* fp, ssize_t (*read)(struct file*, char*, size_t, loff_t*), size_t max_size, struct queue_elem_t** first, struct queue_elem_t** last)
{
	struct queue_elem_t* queue_elem = queue_crt();
	if (queue_elem)
	{
		size_t size = 0;
		*first = NULL;
		while((size != max_size) && (queue_read_msg(fp, read, queue_elem) == queue_elem->size))
		{
			struct queue_elem_t* prev = queue_crt();
			*prev = *queue_elem;
			if (!prev) { queue_del_all(*first); *first = *last = NULL; return -ENOMEM; }
			if (!*first) { *first = *last = prev; }
			else { queue_ins(prev, *first); *first = prev; };
			size++;
		}
		queue_del(queue_elem);
		return size;
	}
	return -ENOMEM;
}

static ssize_t queue_write(struct file* fp, ssize_t (*write)(struct file*, const char*, size_t, loff_t*), struct queue_elem_t* pos)
{
	size_t size = 0;
	for (;pos != NULL; pos = pos->next, size++)
	{
		ssize_t ret = queue_write_msg(fp, write, pos);
		if (ret < 0) return ret;
	}
	return size;
}

