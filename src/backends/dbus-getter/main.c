#include <fcntl.h>
#include <glib.h>
#include <mqueue.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define MAX_MESSAGES 5
#define MAX_MSG_SIZE 1024 //1 Mb
#define TIMEOUT 10        //10 seconds

int main(int argc, char **argv)
{
    int ret     = 0;
    mqd_t queue = {0};
    struct mq_attr attr;
    char *buffer            = NULL;
    struct timespec timeout = {0};

    /*int fd = open("/var/log/error1.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1)
    {
        perror("Ошибка при открытии файла");
        return 1;
    }

    if (dup2(fd, STDERR_FILENO) == -1)
    {
        perror("Ошибка при дублировании дескриптора");
        close(fd);
        return 1;
    }*/

    if (argc != 2)
    {
        fprintf(stderr, "Queue name must be specified\n");
        ret = 10;
        goto end;
    }

    if (argv[1][0] != '/')
    {
        fprintf(stderr, "Queue name must start with '/'\n");
        ret = 2;
        goto end;
    }

    buffer = g_malloc(MAX_MSG_SIZE);
    if (!buffer)
    {
        fprintf(stderr, "Can't allocate %d bytes\n", MAX_MSG_SIZE);
        ret = 3;
        goto end;
    }

    attr.mq_flags   = 0;
    attr.mq_maxmsg  = MAX_MESSAGES;
    attr.mq_msgsize = MAX_MSG_SIZE;
    attr.mq_curmsgs = 0;

    queue = mq_open(argv[1], O_CREAT | O_RDONLY, 0644, &attr);
    if (queue == (mqd_t) -1)
    {
        fprintf(stderr, "Can't open queue \"%s\", errno = %d\n", argv[1], errno);
        ret = 4;
        goto end;
    }

    //send signal, connection established
    printf("connect\n");
    fflush(stdout);

    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += TIMEOUT;

    while (TRUE)
    {
        unsigned int prio = -1;
        ssize_t result    = mq_timedreceive(queue, buffer, MAX_MSG_SIZE, &prio, &timeout);

        if (result == -1)
        {
            if (errno == ETIMEDOUT)
            {
                fprintf(stderr, "TIMEOUT. Can't read message from queue %s\n", argv[1]);
                fflush(stderr);
                ret = 5;
                goto end;
            }
            fprintf(stderr, "Can't read message from queue %s\n", argv[1]);
            fflush(stderr);
            ret = 1;
            goto end;
        }
        if (!strcmp(buffer, "exit"))
        {
            printf("Exiting\n");
            fflush(stdout);
            break;
        }

        //Send answer message via signal
        printf("!! - %s\n", buffer);
        fflush(stdout);

        //Update timeout
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += TIMEOUT;
    }

end:
    /*fprintf(stderr, "Exit code is %d\n", ret);
    fflush(stderr);

    close(fd);*/

    if (buffer)
        g_free(buffer);

    if (queue != 0)
        mq_close(queue);

    if (queue != 0)
        mq_unlink(argv[1]);

    return ret;
}
