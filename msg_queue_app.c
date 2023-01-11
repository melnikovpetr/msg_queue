#include "msg_queue.h"

#include <sys/ioctl.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define CMD_EXIT "0"

#define CMD_POP_ "1"
#define CMD_PUSH "2"
#define CMD_LOAD "3"
#define CMD_A_LD "4"
#define CMD_SAVE "5"
#define CMD_A_SV "6"
#define CMD_STRT "7"
#define CMD_STOP "8"

int read_ch()
{
	int res = getchar();
	if (res != '\n') while (getchar() != '\n');
	return res;
}

int cmd_pop_(int fd)
{
	ssize_t ret;
	char buffer[MAX_MSG_SIZE + 1];

	printf("\e[1;1H\e[2J"); // clear

	do
	{
		printf("Poping message from the kernel module message queue...\n");
		ret = read(fd, buffer, MAX_MSG_SIZE);
		if (ret < 0)
		{
			perror("Failed to read the message from the device");
			read_ch();
		}
		else
		{
			buffer[ret] = '\0';
			printf("The received message is: [%s]\n", buffer);
			printf("One more?: (Y/n) ");
		}
	}
	while((ret >= 0) && ((ret = read_ch()) == 'Y'));
	return ret;
}

int cmd_push(int fd)
{
	ssize_t ret;
    char buffer[MAX_MSG_SIZE + 1];

	printf("\e[1;1H\e[2J"); // clear
	do
	{
		printf("Type in a short string to push to the kernel module message queue:\n");
        scanf("%"_S(MAX_MSG_SIZE)"[^\n]%*c", buffer);
		printf("Pushing message into the queue [%s].\n", buffer);
		ret = write(fd, buffer, strlen(buffer));
		if (ret < 0)
		{
			perror("Failed to write the message to the device");
			read_ch();
		}
		else
		{
			printf("One more?: (Y/n) ");
		}
	}
	while((ret >= 0) && ((ret = read_ch()) == 'Y'));
	return ret;
}

int cmd_load(int fd, int async)
{
	ssize_t ret;
	char buffer[PATH_MAX + 1];

	printf("\e[1;1H\e[2J"); // clear

	printf("Type in the input file path:\n");
	scanf("%"_S(PATH_MAX)"[^\n]%*c", buffer);

    ret = ioctl(fd, async ? MSG_QUEUE_LOAD_ASYNC : MSG_QUEUE_LOAD, buffer);

	if (ret < 0)
	{
		perror("Failed to load messages to the device");
		read_ch();
	}

    if (!async) printf("%zd message(s) have been loaded to the message queue\n", ret);
	printf("Press Enter to continue...\n");
	read_ch();

	return ret;
}

int cmd_save(int fd, int async)
{
	ssize_t ret;
    char buffer[PATH_MAX + 1];

	printf("\e[1;1H\e[2J"); // clear

	printf("Type in the output file path:\n");
    scanf("%"_S(PATH_MAX)"[^\n]%*c", buffer);

    ret = ioctl(fd, async ? MSG_QUEUE_SAVE_ASYNC : MSG_QUEUE_SAVE, buffer);

	if (ret < 0)
	{
		perror("Failed to save messages from the device");
		read_ch();
	}

    if (!async) printf("%zd message(s) have been written to the file\n", ret);
	printf("Press Enter to continue...\n");
	read_ch();

	return ret;
}

int cmd_strt()
{
    printf("\e[1;1H\e[2J"); // clear
    printf("Starting pop service\n");
    int ret = system("sudo systemctl start " DAEMON_NAME ".service");
	printf("Press Enter to continue...\n");
	read_ch();
    return ret;
}

int cmd_stop()
{
    printf("\e[1;1H\e[2J"); // clear
    printf("Stopping pop service\n");
    int ret = system("sudo systemctl stop " DAEMON_NAME ".service");
	printf("Press Enter to continue...\n");
	read_ch();
    return ret;
}

int main()
{
	int fd;
	int ret;
	int cmd;

	printf("Starting device test code example...\n");

    fd = open("/dev/"DEVICE_NAME, O_RDWR | O_NONBLOCK);

	if (fd < 0)
	{
		perror("Failed to open the device...");
		return errno;
	}

	do
	{
		printf("\e[1;1H\e[2J"); // clear

		printf(CMD_POP_ ". Pop message\n");
		printf(CMD_PUSH ". Push message\n");
		printf(CMD_LOAD ". Load messages\n");
        printf(CMD_A_LD ". Load messages asynchronously\n");
        printf(CMD_SAVE ". Save messages\n");
        printf(CMD_A_SV ". Save messages asynchronously\n");
		printf(CMD_STRT ". Start pop service\n");
		printf(CMD_STOP ". Stop pop service\n");

        printf("\n" CMD_EXIT ". Exit\n");

		printf("Choose command for continue: ");

		while((cmd = read_ch()) != *CMD_EXIT)
		{
            if (cmd == *CMD_POP_) { ret = cmd_pop_(fd);    break; } else
            if (cmd == *CMD_PUSH) { ret = cmd_push(fd);    break; } else
            if (cmd == *CMD_LOAD) { ret = cmd_load(fd, 0); break; } else
            if (cmd == *CMD_A_LD) { ret = cmd_load(fd, 1); break; } else
            if (cmd == *CMD_SAVE) { ret = cmd_save(fd, 0); break; } else
            if (cmd == *CMD_A_SV) { ret = cmd_save(fd, 1); break; } else
			if (cmd == *CMD_STRT) { ret = cmd_strt();      break; } else
			if (cmd == *CMD_STOP) { ret = cmd_stop();      break; } else
            {}
		}
	}
	while(cmd != *CMD_EXIT);

	close(fd);

	printf("End of the program\n");
	return 0;
}
