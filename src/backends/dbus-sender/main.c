#include <glib.h>
#include <mqueue.h>
#include <stdio.h>

#define MAX_MESSAGES 10
#define MAX_MSG_SIZE 1024 * 1024 //1Mb

int main(int argc, char **argv)
{
    int ret = 1;

    gchar *message    = NULL;
    gchar *queue_name = NULL;
    mqd_t queue       = {0};
    struct mq_attr attr;

    if (argc < 3)
    {
        fprintf(stderr, "At a minimum, there should be two arguments for which queue to send to and what to send to.\n");
        ret = 1;
        goto end;
    }

    queue_name = argv[1];

    message = g_strdup("");

    for (int i = 2; i < argc; i++)
    {
        gchar *tmp = message;
        if (i == argc - 1)
        {
            message = g_strconcat(message, argv[i], NULL);
        }
        else
        {
            message = g_strconcat(message, argv[i], " ", NULL);
        }
        g_free(tmp);
    }
    printf("To send: %s\n", message);

    attr.mq_flags   = 0;
    attr.mq_maxmsg  = MAX_MESSAGES;
    attr.mq_msgsize = MAX_MSG_SIZE;
    attr.mq_curmsgs = 0;

    queue = mq_open(queue_name, O_CREAT | O_EXCL | O_WRONLY, 0644, &attr);
    if (queue != (mqd_t) -1)
    {
        fprintf(stderr, "Queue &s is not exists.\n");
        ret = 1;
        goto end;
    }

    memset(&queue, 0, sizeof(mqd_t));

    queue = mq_open(queue_name, O_CREAT | O_WRONLY, 0644, &attr);
    if (queue == (mqd_t) -1)
    {
        fprintf(stderr, "Can't open queue: %s.\n", queue_name);
        ret = 1;
        goto end;
    }

    if (mq_send(queue, message, strlen(message) + 1, 0) != 0)
    {
        fprintf(stderr, "Can't send message.\n");
        ret = 1;
        goto end;
    }

end:
    g_free(message);

    mq_close(queue);

    mq_unlink(queue_name);

    return ret;
}
