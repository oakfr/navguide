#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#include <glib.h>

#include "glib_util.h"

#define dbg(args...) fprintf(stderr, args)
#undef dbg
#define dbg(args...)

typedef struct _signal_pipe {
    int fds[2];
    GIOChannel *ioc;
    guint ios;

    signal_pipe_glib_handler_t userfunc;
    void *userdata;
} signal_pipe_t;

static signal_pipe_t g_sp;
static int g_sp_initialized = 0;

int 
signal_pipe_init()
{
    if (g_sp_initialized) {
        fprintf(stderr, "signal_pipe already initialized!!\n");
        return -1;
    }

    if (0 != pipe (g_sp.fds)) {
        perror("signal_pipe");
        return -1;
    }

    int flags = fcntl (g_sp.fds[1], F_GETFL);
    fcntl (g_sp.fds[1], F_SETFL, flags | O_NONBLOCK);

    g_sp_initialized = 1;

    dbg("signal_pipe: initialized\n");
    return 0;
}

int 
signal_pipe_cleanup()
{
    if (g_sp_initialized) {
        close (g_sp.fds[0]);
        close (g_sp.fds[1]);
        g_io_channel_unref (g_sp.ioc);
        g_sp_initialized = 0;
        return 0;
    }

    dbg("signal_pipe: destroyed\n");
    return -1;
}

static void
signal_handler (int signum)
{
    dbg("signal_pipe: caught signal %d\n", signum);
    write (g_sp.fds[1], &signum, sizeof(int));
}

static int
signal_handler_glib (GIOChannel *source, GIOCondition condition, void *ud)
{
    int signum;
    int status;
    status = read (g_sp.fds[0], &signum, sizeof(int));

    if (status != sizeof(int)) {
        fprintf(stderr, "wtf!? signal_handler_glib is confused (%s:%d)\n", 
                __FILE__, __LINE__);
        return TRUE;
    }

    if (g_sp.userfunc) {
        g_sp.userfunc (signum, g_sp.userdata);
    }

    return TRUE;
}

void 
signal_pipe_add_signal (int sig)
{
    // TODO use sigaction instead of signal()
#if 0
    struct sigaction siga;
    siga.sa_handler = signal_handler;
    siga.sa_sigaction = NULL;
    sigemptyset (&siga.sa_mask);
    siga.sa_flags = 0;
    siga.sa_restorer = 0;

    int status = sigaction (sig, &siga, NULL);
    if (0 != status) {
        perror("signal_pipe: sigaction failed");
    }
#else
    signal (sig, signal_handler);
#endif

    return;
}

int 
signal_pipe_attach_glib (signal_pipe_glib_handler_t func, gpointer user_data)
{
    if (! g_sp_initialized) return -1;

    if (g_sp.ioc) return -1;

    g_sp.ioc = g_io_channel_unix_new (g_sp.fds[0]);
    g_io_channel_set_flags (g_sp.ioc, 
                            (GIOFlags)(g_io_channel_get_flags (g_sp.ioc) | G_IO_FLAG_NONBLOCK), NULL);
    g_sp.ios = g_io_add_watch (g_sp.ioc, (GIOCondition)(G_IO_IN | G_IO_PRI), 
            (GIOFunc) signal_handler_glib, NULL);

    g_sp.userfunc = func;
    g_sp.userdata = user_data;

    return 0;
}


static void
spgqok_handler (int signum, void *user)
{
    GMainLoop *mainloop = (GMainLoop*) user;
    g_main_loop_quit (mainloop);
    signal_pipe_cleanup();
}

int 
signal_pipe_glib_quit_on_kill (GMainLoop *mainloop)
{
    if (0 != signal_pipe_init()) return -1;

    signal_pipe_add_signal (SIGINT);
    signal_pipe_add_signal (SIGTERM);
    signal_pipe_add_signal (SIGKILL);
    signal_pipe_add_signal (SIGHUP);

    return signal_pipe_attach_glib (spgqok_handler, mainloop);
}

static int
lcm_message_ready (GIOChannel *source, GIOCondition cond, lcm_t *lcm)
{
    lcm_handle (lcm);
    return TRUE;
}

typedef struct {
    GIOChannel *ioc;
    guint sid;
    lcm_t *lcm;
} glib_attached_lcm_t;

static GHashTable *lcm_glib_sources = NULL;
static GStaticMutex lcm_glib_sources_mutex = G_STATIC_MUTEX_INIT;

int
glib_mainloop_attach_lcm (lcm_t *lcm)
{
    g_static_mutex_lock (&lcm_glib_sources_mutex);

    if (!lcm_glib_sources) {
        lcm_glib_sources = g_hash_table_new (g_direct_hash, g_direct_equal);
    }

    if (g_hash_table_lookup (lcm_glib_sources, lcm)) {
        dbg ("LC %p already attached to mainloop\n", lcm);
        g_static_mutex_unlock (&lcm_glib_sources_mutex);
        return -1;
    }

    glib_attached_lcm_t *galcm = 
        (glib_attached_lcm_t*) calloc (1, sizeof (glib_attached_lcm_t));

    galcm->ioc = g_io_channel_unix_new (lcm_get_fileno (lcm));
    galcm->sid = g_io_add_watch (galcm->ioc, G_IO_IN, (GIOFunc) lcm_message_ready, 
            lcm);
    galcm->lcm = lcm;

    dbg ("inserted LC %p into glib mainloop\n", lcm);
    g_hash_table_insert (lcm_glib_sources, lcm, galcm);

    g_static_mutex_unlock (&lcm_glib_sources_mutex);
    return 0;
}

int
glib_mainloop_detach_lcm (lcm_t *lcm)
{
    g_static_mutex_lock (&lcm_glib_sources_mutex);
    if (!lcm_glib_sources) {
        dbg ("no lcm glib sources\n");
        g_static_mutex_unlock (&lcm_glib_sources_mutex);
        return -1;
    }

    glib_attached_lcm_t *galcm = 
        (glib_attached_lcm_t*) g_hash_table_lookup (lcm_glib_sources, lcm);

    if (!galcm) {
        dbg ("couldn't find matching gaLC\n");
        g_static_mutex_unlock (&lcm_glib_sources_mutex);
        return -1;
    }

    dbg ("detaching LC from glib\n");
    g_io_channel_unref (galcm->ioc);
    g_source_remove (galcm->sid);

    g_hash_table_remove (lcm_glib_sources, lcm);
    free (galcm);

    if (g_hash_table_size (lcm_glib_sources) == 0) {
        g_hash_table_destroy (lcm_glib_sources);
        lcm_glib_sources = NULL;
    }

    g_static_mutex_unlock (&lcm_glib_sources_mutex);
    return 0;
}

double m_timer_elapsed (GTimer *timer)
{
    gulong usecs;
    double secs = g_timer_elapsed (timer, &usecs);
    g_timer_destroy (timer);
    return secs;
}

