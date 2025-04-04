#include <fcntl.h>
#include <glib.h>
#include <mqueue.h>
#include <stdio.h>
#include <unistd.h>

#define MAX_MESSAGES 5
#define MAX_MSG_SIZE 1024 //1Mb

int main(int argc, char **argv)
{
    int ret = 0;

    gchar *message    = NULL;
    gchar *queue_name = NULL;
    mqd_t queue       = {0};
    struct mq_attr attr;

    /*int fd = open("/var/log/error2.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
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

    attr.mq_flags   = 0;
    attr.mq_maxmsg  = MAX_MESSAGES;
    attr.mq_msgsize = MAX_MSG_SIZE;
    attr.mq_curmsgs = 0;

    queue = mq_open(queue_name, O_CREAT | O_EXCL | O_WRONLY, 0644, &attr);
    if (queue != (mqd_t) -1)
    {
        fprintf(stderr, "Queue %s is not exists.\n", queue_name);
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

    /*fprintf(stderr, "Exit code is %d\n", ret);
    fflush(stderr);

    close(fd);*/
    return ret;
}
