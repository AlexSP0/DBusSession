#include <gio/gio.h>
#include <glib-object.h>
#include <glib/gvariant.h>
#include <stdatomic.h>
#include <stdio.h>

#define QUEUE_NAME "/mqueue"
#define SERVICE "org.altlinux.alterator"
#define PATH "/org/altlinux/alterator/interactive_example"
#define INTERFACE "org.altlinux.alterator.interactive"
#define RUN_METHOD "Run"
#define SEND_METHOD "Send"

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

static int call_run_method(const char *queue)
{
    int ret                     = 0;
    GDBusConnection *connection = NULL;
    GError *error               = NULL;
    GVariant *parameters        = NULL;
    GVariant *result            = NULL;

    connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (!connection)
    {
        fprintf(stderr, "ERROR: can't create GDBusSource\n");
        ret = 1;
        goto end;
    }

    result = g_dbus_connection_call_sync(
        connection, SERVICE, PATH, INTERFACE, RUN_METHOD, parameters, NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

end:
    if (result)
        g_variant_unref(result);

    if (parameters)
        g_variant_unref(parameters);

    if (result)
        g_variant_unref(result);

    if (error)
        g_error_free(error);

    if (connection)
        g_object_unref(connection);

    return ret;
}
static int call_send_method(const char *queue, const char *message);
static int subscribe_signals();
static int stdout_signal(const char *message);
static int stderr_signal(const char *message);

static void *connector_func(void *)
{
    int *ret = malloc(sizeof(int));
    if (!ret)
        pthread_exit(NULL);

    *ret = 0;

    int status = -1;

    status = atomic_load(&conn_status);

    if (status != NO_CONNECT)
        return (void *) 1;

    atomic_store(&conn_status, TRY_TO_CONNECT);

    if (call_run_method((char *) QUEUE_NAME) != 0)
        return (void *) 1;

    atomic_store(&conn_status, NO_CONNECT);
end:
    pthread_exit(ret);
}

static void *worker_func(void *)
{
    int *ret = malloc(sizeof(int));
    if (!ret)
        pthread_exit(NULL);

    *ret = 0;

    while (true)
    {
    }

end:
    pthread_exit(ret);
}

int main()
{
    int ret                = 0;
    void *connector_status = 0;
    void *worker_status    = 0;
    int c                  = 0;
    int w                  = 0;

    if (pthread_create(&worker, NULL, &worker_func, NULL) != 0)
    {
        fprintf(stderr, "ERROR: can't create worker thread\n");
        ret = 1;
        goto end;
    }

    if (pthread_create(&connector, NULL, &connector_func, NULL) != 0)
    {
        fprintf(stderr, "ERROR: can't create connector thread\n");
        ret = 1;
        goto end;
    }

end:
    if (connector)
    {
        pthread_join(connector, &connector_status);
        if (!connector_status)
        {
            c = *((int *) connector_status);
            printf("connector status: %d\n", c);
            free(connector_status);
        }
    }

    if (worker)
    {
        pthread_join(worker, &worker_status);
        if (!worker_status)
        {
            w = *((int *) worker_status);
            printf("worker status: %d\n", w);
            free(worker_status);
        }
    }

    return ret;
}
