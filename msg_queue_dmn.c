#include "msg_queue.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>

ssize_t pop_queue(int in, int out)
{
    ssize_t ret;
    char buffer[MAX_MSG_SIZE];

    ret = read(in, buffer, MAX_MSG_SIZE);
    if (ret >= 0)
    {
        ssize_t w_ret;

        if (((w_ret = write(out, &ret, sizeof(ret))) < 0)
            || ((w_ret = write(out, buffer, ret)) < 0))
        {
            syslog(LOG_ALERT, "failed to write storage file (error code: [%zd])", w_ret);
            return w_ret;
        }
	}

    return ret;
}

int main(int argc, char* argv[])
{
    ssize_t ret;
    pid_t pid;
    pid_t sid;
    int in;
    int out;

    pid = fork();

    if (pid < 0) { exit(EXIT_FAILURE); }
    if (pid > 0) { exit(EXIT_SUCCESS); }

    umask(0);

    openlog(DAEMON_NAME, LOG_NDELAY | LOG_PID, LOG_DAEMON);

    sid = setsid();

    if (sid < 0)
    {
        syslog(LOG_ALERT, "session id failure");
        exit(EXIT_FAILURE);
    }

    if ((chdir("/")) < 0)
    {
        syslog(LOG_ALERT, "change directory failure");
        exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    in = open("/dev/"DEVICE_NAME, O_RDONLY);

    if (in < 0)
    {
        syslog(LOG_ALERT, "failed to open the device");
        exit(EXIT_FAILURE);
    }

    if (argc != 2)
    {
        syslog(LOG_ALERT, "incorrect number of arguments");
        exit(EXIT_FAILURE);
    }

    out = open(argv[1], O_CREAT | O_WRONLY | O_APPEND, 0666);

    if (out < 0)
    {
        syslog(LOG_ALERT, "failed to open the the storage file");
        exit(EXIT_FAILURE);
    }

    while(1)
    {
        ret = pop_queue(in, out);
		if (ret < 0)
		{
			ret = errno;
			if (ret == EEMPTY) sleep(30);
			else break;
		}
	}

    close(out);
    close(in);
    closelog();

    return ret;
}
