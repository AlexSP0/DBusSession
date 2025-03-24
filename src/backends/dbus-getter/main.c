#include <glib.h>
#include <mqueue.h>
#include <stdio.h>

#define MAX_MESSAGES 10
#define MAX_MSG_SIZE 1024 * 2 //2Mb

int main(int argc, char **argv)
{
    int ret     = 0;
    mqd_t queue = {0};
    struct mq_attr attr;
    char *buffer = NULL;

    if (argc != 2)
    {
        fprintf(stderr, "Queue name must be specified\n");
        ret = 1;
        goto end;
    }

    buffer = g_malloc(MAX_MSG_SIZE + 1);
    if (!buffer)
    {
        fprintf(stderr, "Can't allocate %d bytes\n", MAX_MSG_SIZE + 1);
        ret = 1;
        goto end;
    }

    attr.mq_flags   = 0;
    attr.mq_maxmsg  = MAX_MESSAGES;
    attr.mq_msgsize = MAX_MSG_SIZE;
    attr.mq_curmsgs = 0;

    queue = mq_open(argv[1], O_CREAT | O_RDONLY, 0644, &attr);
    if (queue == (mqd_t) -1)
    {
        fprintf(stderr, "Can't open queue %s\n", argv[1]);
        ret = 1;
        goto end;
    }

    while (TRUE)
    {
        unsigned int prio = -1;
        if (mq_receive(queue, buffer, MAX_MSG_SIZE, &prio) == -1)
        {
            fprintf(stderr, "Can't read message from queue %s\n", argv[1]);
            ret = 1;
            goto end;
        }
        if (!strcmp(buffer, "exit"))
        {
            printf("Exiting\n");
            break;
        }

        printf("%s\n", buffer);
        fflush(stdout);
    }

end:
    if (buffer)
        g_free(buffer);

    mq_close(queue);

    return ret;
}
