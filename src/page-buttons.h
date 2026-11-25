#ifdef HNINK_ARM_IMPORTS
/* Linux ARM EABI input_event and EVIOCGBIT for the sysroot-free build. */
struct input_event {
    long seconds, microseconds;
    unsigned short type, code;
    int value;
};
#define EV_KEY 1
#define KEY_A 30
#define KEY_PAGEUP 104
#define KEY_PAGEDOWN 109
#define EVIOCGBIT(event, length) (0x80004520UL | (event) | ((length) << 16))
#define O_RDONLY 0
#define O_NONBLOCK 04000
#define O_CLOEXEC 02000000
#else
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#endif

/* Match the Oasis button order with the display upright. */
static int page_button_direction(const struct input_event *event) {
    if (event->type != EV_KEY || event->value != 1) return 0;
    if (event->code == KEY_PAGEUP) return -1;
    if (event->code == KEY_PAGEDOWN) return 1;
    return 0;
}

static void apply_page_buttons(App *app) {
    GString *steps = app->button_steps;
    app->button_steps = 0;
    if (app->revision == app->button_revision && gtk_window_has_toplevel_focus(GTK_WINDOW(app->window))) {
        int forward = app->buttons_reversed != app->reverse_page_buttons ? -1 : 1;
        for (gsize i = 0; i < steps->len; ++i)
            navigate(app, steps->str[i] == '+' ? forward : -forward);
    }
    g_string_free(steps, TRUE);
}

static gboolean read_button_orientation(GIOChannel *channel, GIOCondition condition, gpointer data) {
    App *app = data;
    char *orientation = 0;
    GIOStatus result = g_io_channel_read_line(channel, &orientation, 0, 0, 0);
    if (result == G_IO_STATUS_AGAIN && !(condition & (G_IO_HUP | G_IO_ERR))) return TRUE;
    if (orientation) {
        g_strchomp(orientation);
        if (g_str_equal(orientation, "D")) app->buttons_reversed = TRUE;
        else if (g_str_equal(orientation, "U")) app->buttons_reversed = FALSE;
        g_free(orientation);
    }
    app->button_orientation_watch = 0;
    apply_page_buttons(app);
    return FALSE;
}

static void queue_page_button(App *app, int direction) {
    if (!app->button_steps || app->button_revision != app->revision) {
        if (app->button_steps) g_string_free(app->button_steps, TRUE);
        app->button_steps = g_string_new(0);
        app->button_revision = app->revision;
    }
    g_string_append_len(app->button_steps, direction > 0 ? "+" : "-", 1);
    if (app->button_orientation_watch) return;

    /* Read the displayed orientation so screen locks and rotation agree with the buttons. */
    char *argv[] = {"lipc-get-prop", "com.lab126.winmgr", "orientation", 0};
    GPid pid;
    gint fd;
    if (!g_spawn_async_with_pipes(0, argv, 0,
        G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_STDERR_TO_DEV_NULL,
        0, 0, &pid, 0, &fd, 0, 0)) {
        apply_page_buttons(app);
        return;
    }
    g_child_watch_add(pid, reap_child, 0);
    GIOChannel *channel = g_io_channel_unix_new(fd);
    g_io_channel_set_close_on_unref(channel, TRUE);
    g_io_channel_set_flags(channel, G_IO_FLAG_NONBLOCK, 0);
    app->button_orientation_watch = g_io_add_watch_full(channel, G_PRIORITY_DEFAULT_IDLE,
        G_IO_IN | G_IO_HUP | G_IO_ERR, read_button_orientation, app, 0);
    g_io_channel_unref(channel);
}

static gboolean read_page_buttons(GIOChannel *channel, GIOCondition condition, gpointer data) {
    App *app = data;
    if (condition & G_IO_IN) {
        struct input_event events[16];
        gsize size = 0;
        GIOStatus status = g_io_channel_read_chars(channel, (char *)events, sizeof(events), &size, 0);
        if (status == G_IO_STATUS_AGAIN) return TRUE;
        if (status == G_IO_STATUS_NORMAL && size) {
            for (gsize i = 0; i < size / sizeof(events[0]); ++i) {
                int direction = page_button_direction(&events[i]);
                if (direction && gtk_window_has_toplevel_focus(GTK_WINDOW(app->window)))
                    queue_page_button(app, direction);
            }
            return TRUE;
        }
    }
    app->page_buttons_watch = 0;
    return FALSE;
}

static guint watch_page_buttons(App *app) {
    GDir *devices = g_dir_open("/dev/input", 0, 0);
    if (!devices) return 0;
    guint watch = 0;
    const char *name;
    while (!watch && (name = g_dir_read_name(devices))) {
        if (!g_str_has_prefix(name, "event")) continue;
        char *path = g_build_filename("/dev/input", name, (const char *)0);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        g_free(path);
        if (fd < 0) continue;
        GIOChannel *channel = g_io_channel_unix_new(fd);
        g_io_channel_set_close_on_unref(channel, TRUE);
        unsigned char keys[KEY_PAGEDOWN / 8 + 1] = {0};
        if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) >= 0 &&
            !(keys[KEY_A / 8] & (1u << (KEY_A % 8))) &&
            (keys[KEY_PAGEUP / 8] & (1u << (KEY_PAGEUP % 8))) &&
            (keys[KEY_PAGEDOWN / 8] & (1u << (KEY_PAGEDOWN % 8)))) {
            g_io_channel_set_encoding(channel, 0, 0);
            g_io_channel_set_buffered(channel, FALSE);
            watch = g_io_add_watch_full(channel, G_PRIORITY_DEFAULT_IDLE,
                G_IO_IN | G_IO_HUP | G_IO_ERR, read_page_buttons, app, 0);
        }
        g_io_channel_unref(channel);
    }
    g_dir_close(devices);
    return watch;
}
