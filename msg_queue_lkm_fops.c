#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/buffer_head.h>

/*
	if (IS_ERR(filp))
	{
		err = PTR_ERR(filp);
		return NULL;
	}
*/

static struct file* file_open(const char* path, int flags, int rights)
{
	return filp_open(path, flags, rights);
}

static void file_close(struct file* fp)
{
	filp_close(fp, NULL);
}

static ssize_t file_read(struct file* fp, char* buffer, size_t len, loff_t* off)
{
	mm_segment_t oldfs;
	int ret;

	oldfs = get_fs();
	set_fs(get_ds());

	ret = vfs_read(fp, buffer, len, off);

	set_fs(oldfs);

	return ret;
}

static ssize_t file_write(struct file* fp, const char* buffer, size_t len, loff_t* off)
{
	mm_segment_t oldfs;
	int ret;

	oldfs = get_fs();
	set_fs(get_ds());

	ret = vfs_write(fp, buffer, len, off);

	set_fs(oldfs);

	return ret;
}
