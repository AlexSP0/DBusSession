#include <fcntl.h>
#include <gio/gio.h>
#include <glib-object.h>
#include <glib/gvariant.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#define QUEUE_NAME "/mqueue"
#define SERVICE "org.altlinux.alterator"
#define PATH "/org/altlinux/alterator/interactive_example"
#define INTERFACE "org.altlinux.alterator.interactive"
#define RUN_METHOD "Run"
#define SEND_METHOD "Send"
#define SIGNAL_STDOUT_NAME "interactive_stdout_signal"
#define SIGNAL_STDERR_NAME "interactive_stderr_signal"
#define BUFFER_SIZE 1000

typedef enum CONN_STATUS
{
    NO_CONNECT,
    TRY_TO_CONNECT,
    CONNECTED,
    EXITING,
} CONN_STATUS;

atomic_int conn_status = NO_CONNECT;
pthread_t connector    = 0;
pthread_t worker       = 0;
GMainLoop *main_loop   = {0};

static int call_send_method(const char *message);
static int subscribe_signals();
static int stdout_signal(const char *message);
static int stderr_signal(const char *message);
static gchar *get_signal_name(gchar *interface_name);
gchar *concat_signal_name_with_connection_name(const gchar *signal, const gchar *connection);
static void run_stdout_signal_handler(GDBusConnection *connection,
                                      const gchar *sender_name,
                                      const gchar *object_path,
                                      const gchar *interface_name,
                                      const gchar *signal_name,
                                      GVariant *parameters,
                                      gpointer user_data);

static void run_stderrsignal_handler(GDBusConnection *connection,
                                     const gchar *sender_name,
                                     const gchar *object_path,
                                     const gchar *interface_name,
                                     const gchar *signal_name,
                                     GVariant *parameters,
                                     gpointer user_data);

static void run_stdout_signal_handler(GDBusConnection *connection,
                                      const gchar *sender_name,
                                      const gchar *object_path,
                                      const gchar *interface_name,
                                      const gchar *signal_name,
                                      GVariant *parameters,
                                      gpointer user_data)
{
    const gchar *message = NULL;

    int status = atomic_load(&conn_status);

    if (status == TRY_TO_CONNECT)
    {
        atomic_store(&conn_status, CONNECTED);
    }
    else if (status == NO_CONNECT || status == EXITING)
    {
        goto end;
    }

    g_variant_get(parameters, "(s)", &message);
    printf("STDOUT - Получено сообщение: %s\n", message);
    fflush(stdout);

end:
    return;
}

static void run_stderr_signal_handler(GDBusConnection *connection,
                                      const gchar *sender_name,
                                      const gchar *object_path,
                                      const gchar *interface_name,
                                      const gchar *signal_name,
                                      GVariant *parameters,
                                      gpointer user_data)
{
    const gchar *message = NULL;

    int status = atomic_load(&conn_status);

    if (status == TRY_TO_CONNECT)
    {
        atomic_store(&conn_status, CONNECTED);
    }
    else if (status == NO_CONNECT || status == EXITING)
    {
        goto end;
    }

    g_variant_get(parameters, "(s)", &message);
    printf("STDERR - Получено сообщение: %s\n", message);
    fflush(stdout);

    //Timeout from getter
    if (!strstr("TIMEOUT", message))
    {
        atomic_store(&conn_status, EXITING);
    }

end:
    return;
}

gchar *concat_signal_name_with_connection_name(const gchar *signal, const gchar *connection)
{
    if (!signal || !connection)
        return NULL;

    size_t signal_size     = strlen(signal);
    size_t connection_size = strlen(connection);

    char fixed_conn[255] = {0};
    strcpy(fixed_conn, connection);

    char *substring_ptr = strstr(fixed_conn, ":");
    if (substring_ptr)
    {
        size_t position      = substring_ptr - fixed_conn;
        fixed_conn[position] = '_';
    }

    substring_ptr = strstr(fixed_conn, ".");
    if (substring_ptr)
    {
        size_t position      = substring_ptr - fixed_conn;
        fixed_conn[position] = '_';
    }

    gchar *result = g_malloc(signal_size + connection_size + 1);
    memset(result, 0, signal_size + connection_size + 1);

    strcat(result, signal);
    strcat(result, fixed_conn);

    return result;
}
static int call_send_method(const char *message)
{
    int ret                        = 0;
    gint exit_code                 = -1;
    GVariant *result               = NULL;
    GVariant *parameters           = NULL;
    const GVariantType *reply_type = NULL;
    GError *error                  = NULL;
    GDBusConnection *connection    = NULL;

    connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (!connection)
    {
        fprintf(stderr, "ERROR: can't get dbus connection\n");
        ret = 1;
        goto end;
    }

    parameters = g_variant_new("(ss)", QUEUE_NAME, message);
    reply_type = G_VARIANT_TYPE("(i)");

    result = g_dbus_connection_call_sync(
        connection, SERVICE, PATH, INTERFACE, SEND_METHOD, parameters, NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

    if (!result)
    {
        fprintf(stderr, "ERROR: can't get reply from send() method\n");
        fprintf(stderr, "ERROR: %s\n", error->message);
        atomic_store(&conn_status, EXITING);
        ret = 1;
        goto end;
    }

    if (!g_variant_type_equal(g_variant_get_type(result), reply_type))
    {
        fprintf(stderr, "ERROR: Wrong answer type from send() method\n");
        fprintf(stderr, "ERROR: %s\n", error->message);
        atomic_store(&conn_status, EXITING);
        ret = 1;
        goto end;
    }

    g_variant_get(result, "(i)", &exit_code);
    if (exit_code != 0)
    {
        atomic_store(&conn_status, EXITING);
        ret = 1;
        goto end;
    }

end:
    //if (parameters)
    //  g_variant_unref(parameters);

    if (result)
        g_variant_unref(result);

    if (connection)
        g_object_unref(connection);

    if (error)
        g_error_free(error);

    return ret;
}

static int call_run_method(const char *queue)
{
    int ret                        = 0;
    GDBusConnection *connection    = NULL;
    GError *error                  = NULL;
    GDBusProxy *proxy              = NULL;
    GVariant *parameters           = NULL;
    const GVariantType *reply_type = NULL;
    GVariant *result               = NULL;
    gchar *stderr_signal_name      = NULL;
    gchar *stdout_signal_name      = NULL;
    gint stdout_sig_handler        = -1;
    gint stderr_sig_handler        = -1;
    gint exit_code                 = -1;

    connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (!connection)
    {
        fprintf(stderr, "ERROR: can't get dbus connection\n");
        ret = 1;
        goto end;
    }

    //connect signals
    const gchar *dbus_connection_name = g_dbus_connection_get_unique_name(connection); //nonfree
    if (!dbus_connection_name)
    {
        fprintf(stderr, "ERROR: can't get connection name\n");
        ret = 1;
        goto end;
    }
    stderr_signal_name = concat_signal_name_with_connection_name(SIGNAL_STDERR_NAME, dbus_connection_name);
    stdout_signal_name = concat_signal_name_with_connection_name(SIGNAL_STDOUT_NAME, dbus_connection_name);

    stdout_sig_handler = g_dbus_connection_signal_subscribe(connection,
                                                            NULL,
                                                            INTERFACE,
                                                            stdout_signal_name,
                                                            PATH,
                                                            NULL,
                                                            G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                            (gpointer) &run_stdout_signal_handler,
                                                            NULL, //gpointer user-data
                                                            NULL);

    stderr_sig_handler = g_dbus_connection_signal_subscribe(connection,
                                                            NULL,
                                                            INTERFACE,
                                                            stderr_signal_name,
                                                            PATH,
                                                            NULL,
                                                            G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                            (gpointer) &run_stderr_signal_handler,
                                                            NULL, //gpointer user-data
                                                            NULL);

    proxy = g_dbus_proxy_new_sync(connection, G_DBUS_PROXY_FLAGS_NONE, NULL, SERVICE, PATH, INTERFACE, NULL, &error);
    if (!proxy)
    {
        fprintf(stderr, "ERROR: can't create dbus proxy\n");
        ret = 1;
        goto end;
    }

    parameters = g_variant_new("(s)", QUEUE_NAME);
    reply_type = G_VARIANT_TYPE("(i)");

    //TODO set timeout
    result = g_dbus_proxy_call_sync(proxy, RUN_METHOD, parameters, G_DBUS_CALL_FLAGS_NONE, G_MAXINT, NULL, &error);

    atomic_store(&conn_status, NO_CONNECT);

    //Disconnect signals
    g_dbus_connection_signal_unsubscribe(connection, stdout_sig_handler);
    g_dbus_connection_signal_unsubscribe(connection, stderr_sig_handler);

    if (!result)
    {
        fprintf(stderr, "ERROR: can't get reply from run() method - %s\n", error->message);
        ret = 1;
        goto end;
    }

    if (!g_variant_type_equal(g_variant_get_type(result), reply_type))
    {
        fprintf(stderr, "ERROR: Wrong answer type from run() method\n");
        ret = 1;
        goto end;
    }

    g_variant_get(result, "(i)", &exit_code);
    if (exit_code != 0)
    {
        ret = 1;
        goto end;
    }

end:
    atomic_store(&conn_status, EXITING);

    g_main_loop_quit(main_loop);

    g_free(stderr_signal_name);

    g_free(stdout_signal_name);

    if (result)
        g_variant_unref(result);

    //if (parameters)
    //  g_variant_unref(parameters);

    if (error)
        g_error_free(error);

    if (connection)
        g_object_unref(connection);

    return ret;
}

static void *connector_func(void *)
{
    int *ret = malloc(sizeof(int));
    if (!ret)
        pthread_exit(NULL);

    *ret = 0;

    int status = -1;

    status = atomic_load(&conn_status);

    if (status != NO_CONNECT)
    {
        *ret = 1;
        goto end;
    }

    atomic_store(&conn_status, TRY_TO_CONNECT);

    if (call_run_method((char *) QUEUE_NAME) != 0)
    {
        *ret = 1;
        fflush(stdout);
        goto end;
    }

    atomic_store(&conn_status, NO_CONNECT);
end:
    pthread_exit(ret);
}

static void *worker_func(void *)
{
    int *ret                 = malloc(sizeof(int));
    char buffer[BUFFER_SIZE] = {0};

    if (!ret)
        pthread_exit(NULL);

    *ret = 0;

    //Nonblocking fgets

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    while (TRUE)
    {
        int conn_st = atomic_load(&conn_status);
        if (conn_st == EXITING)
            break;

        if (conn_st != CONNECTED)
        {
            sched_yield();
            continue;
        }

        //Connected. Do work
        printf("Input message or type \"exit\" to go out:\n");
        fflush(stdout);

        while (TRUE)
        {
            int status = atomic_load(&conn_status);

            if (status != CONNECTED)
                goto end;

            if (fgets(buffer, BUFFER_SIZE, stdin) != NULL)
            {
                //Remove '\n'
                size_t len = strlen(buffer);
                if (len > 0 && buffer[len - 1] == '\n')
                {
                    buffer[len - 1] = '\0';
                }

                if (strlen(buffer) == 0)
                {
                    continue;
                }

                if (strstr("Exit", buffer))
                {
                    atomic_store(&conn_status, EXITING);
                    printf("Exiting\n");
                    continue;
                }

                //send message
                if (call_send_method(buffer) != 0)
                {
                    fprintf(stderr, "ERROR: can't send message\n");

                    atomic_store(&conn_status, EXITING);

                    *ret = 1;
                }
            }
        }
    }

end:
    fcntl(STDIN_FILENO, F_SETFL, flags);

    pthread_exit(ret);
}

int main()
{
    int ret                = 0;
    void *connector_status = 0;
    void *worker_status    = 0;

    fflush(stdout);
    if (pthread_create(&worker, NULL, &worker_func, NULL) != 0)
    {
        fprintf(stderr, "ERROR: can't create worker thread\n");
        ret = 1;
        goto end;
    }

    fflush(stdout);
    if (pthread_create(&connector, NULL, &connector_func, NULL) != 0)
    {
        fprintf(stderr, "ERROR: can't create connector thread\n");
        ret = 1;
        goto end;
    }

    main_loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(main_loop);

end:
    g_main_loop_unref(main_loop);

    if (connector)
    {
        pthread_join(connector, &connector_status);
        /*if (connector_status)
        {
            c = *((int *) connector_status);
            printf("connector status: %d\n", c);
            free(connector_status);
        }*/
    }

    if (worker)
    {
        pthread_join(worker, &worker_status);
        /*if (worker_status)
        {
            w = *((int *) worker_status);
            printf("worker status: %d\n", w);
            free(worker_status);
        }*/
    }
    return ret;
}
