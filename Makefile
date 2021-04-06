obj-m+=msg_queue_lkm.o

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
	$(CC) msg_queue_app.c -o msg_queue_app
	$(CC) msg_queue_dmn.c -o msg_queue_dmn

clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	rm msg_queue_app
	rm msg_queue_dmn
