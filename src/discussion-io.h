typedef struct {
    App *app;
    GString *buffer;
    gsize scanned;
    gboolean received;
} DiscussionInput;

static void free_discussion_input(gpointer data) {
    DiscussionInput *input = data;
    g_string_free(input->buffer, TRUE);
    g_free(input);
}

static void stop_discussion_read(App *app) {
    if (app->discussion_watch) g_source_remove(app->discussion_watch);
    app->discussion_watch = 0;
}

static gboolean read_discussion(GIOChannel *pipe, GIOCondition condition, gpointer data) {
    DiscussionInput *input = data;
    App *app = input->app;
    char chunk[32768];
    gsize length = 0;
    GIOStatus result = g_io_channel_read_chars(pipe, chunk, sizeof(chunk), &length, 0);
    if (length) g_string_append_len(input->buffer, chunk, length);
    GString *buffer = input->buffer;
    gsize start = 0;
    gsize complete = 0;
    char *frame = 0;
    for (gsize i = input->scanned; i < buffer->len; ++i) {
        if (i > start && buffer->str[i] == '\n' && buffer->str[i - 1] == '.' &&
            (i == start + 1 || buffer->str[i - 2] == '\n')) {
            g_free(frame);
            frame = g_strndup(buffer->str + start, i - 1 - start);
            complete = i + 1;
            start = complete;
        }
    }
    if (frame) {
        g_string_erase(buffer, 0, complete);
        if (!app->notice || !g_str_equal(app->notice, "No comments yet.") ||
            !app->last_output || !g_str_equal(app->last_output, frame)) say(app, "");
        input->received = TRUE;
        render_list(app, frame, TRUE);
    }
    input->scanned = buffer->len;
    if (result == G_IO_STATUS_NORMAL || length || (result == G_IO_STATUS_AGAIN && !(condition & G_IO_HUP)))
        return TRUE;
    if (!input->received) say(app, "Discussion unavailable. Reopen it to try again.");
    app->discussion_watch = 0;
    for (int i = 0; i < app->comment_count; ++i) {
        if (app->comment_rows[i].placeholder_text != loading_comment) continue;
        remember_comment_position(app);
        mount_discussion(app, app->last_output);
        break;
    }
    return FALSE;
}

static void reap_child(GPid pid, gint status, gpointer data) {
    (void)status;
    (void)data;
    g_spawn_close_pid(pid);
}

static void start_discussion_read(App *app) {
    stop_discussion_read(app);
    say(app, loading_comment);
    char *id = g_strdup_printf("%d", app->comment_id[app->depth]);
    char *argv[] = {(char *)app->backend, (char *)app->cache, "discussion", id, "cached", 0};
    GPid pid;
    gint fd;
    gboolean started = g_spawn_async_with_pipes(0, argv, 0,
        G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_STDERR_TO_DEV_NULL, 0, 0, &pid, 0, &fd, 0, 0);
    g_free(id);
    if (!started) {
        say(app, "Couldn’t read the discussion.");
        return;
    }
    g_child_watch_add(pid, reap_child, 0);
    GIOChannel *pipe = g_io_channel_unix_new(fd);
    g_io_channel_set_close_on_unref(pipe, TRUE);
    g_io_channel_set_encoding(pipe, 0, 0);
    g_io_channel_set_buffered(pipe, FALSE);
    g_io_channel_set_flags(pipe, G_IO_FLAG_NONBLOCK, 0);
    DiscussionInput *input = g_malloc0(sizeof(*input));
    input->app = app;
    input->buffer = g_string_new(0);
    app->discussion_watch = g_io_add_watch_full(pipe, G_PRIORITY_DEFAULT_IDLE, G_IO_IN | G_IO_HUP | G_IO_ERR,
        read_discussion, input, free_discussion_input);
    g_io_channel_unref(pipe);
}
