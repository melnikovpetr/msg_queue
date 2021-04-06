#ifndef MSG_QUEUE_H
#define MSG_QUEUE_H

#include <linux/ioctl.h>
#include <linux/limits.h>

#define _S(s) __S(s)
#define __S(s) #s

#define DEVICE_NAME "msg_queue_dev"
#define DAEMON_NAME "msg_queue_dmn"

#define MAX_MSG_SIZE   65536
#define MAX_QUEUE_SIZE 1024

#define EEMPTY 29 // ESPIPE /* Illegal seek */
#define EFULL  27 // EFBIG  /* File too large */

#define MSG_QUEUE_MAGIC_NO 'P'

#define MSG_QUEUE_LOAD       _IOR(MSG_QUEUE_MAGIC_NO, 0, char*)
#define MSG_QUEUE_SAVE       _IOR(MSG_QUEUE_MAGIC_NO, 1, char*)
#define MSG_QUEUE_LOAD_ASYNC _IOR(MSG_QUEUE_MAGIC_NO, 2, char*)
#define MSG_QUEUE_SAVE_ASYNC _IOR(MSG_QUEUE_MAGIC_NO, 3, char*)

#endif // MSG_QUEUE_H
