static void update_bookmark(App *app) {
    gboolean saved = app->discussion_saved;
    GtkWidget *icon = gtk_bin_get_child(GTK_BIN(app->discussion_save));
    g_object_set_data(G_OBJECT(icon), "hn-icon", GINT_TO_POINTER(saved ? ICON_SAVED : ICON_SAVE));
    action_name(app->discussion_save, saved ? "Remove from Saved" : "Save story");
    gtk_widget_set_sensitive(app->discussion_save, !app->account_watch);
    gtk_widget_queue_draw(icon);
}

static void load_account(App *app) {
    char *result = local_command(app, "account", "", 0, 0, 0);
    if (result && g_str_has_prefix(result, "account\t")) {
        char *username = decode(g_strchomp(result + 8));
        if (*username) app->username = username;
        else g_free(username);
    }
    g_free(result);
}

static gboolean read_account_reply(GIOChannel *channel, GIOCondition condition, gpointer data) {
    App *app = data;
    char *line = 0;
    GIOStatus status = g_io_channel_read_line(channel, &line, 0, 0, 0);
    if (status == G_IO_STATUS_AGAIN && !(condition & (G_IO_HUP | G_IO_ERR))) return TRUE;

    app->account_watch = 0;
    if (app->screen == OPTIONS && app->login_button) gtk_widget_set_sensitive(app->login_button, TRUE);
    if (line && g_str_has_prefix(line, "account\t")) {
        g_free(app->username);
        app->username = decode(g_strchomp(line + 8));
        if (!*app->username) {
            g_free(app->username);
            app->username = 0;
        }
        if (app->screen == OPTIONS) {
            say(app, "");
            render(app, 0);
        }
    } else if (line && g_str_has_prefix(line, "favorite\t")) {
        if (app->story == app->account_story && app->screen == COMMENTS)
            say(app, "Saved and added to Hacker News Favorites.");
    } else {
        char *message = 0;
        if (line && g_str_has_prefix(line, "expired\t")) {
            g_free(app->username);
            app->username = 0;
            message = decode(g_strchomp(line + 8));
            if (app->screen == OPTIONS) render(app, 0);
        } else if (line && g_str_has_prefix(line, "error\t")) {
            message = decode(g_strchomp(line + 6));
        }
        if (app->screen == OPTIONS || (app->screen == COMMENTS && app->story == app->account_story))
            say(app, message ? message : "Couldn’t finish the Hacker News request.");
        g_free(message);
    }
    if (app->screen == COMMENTS) update_bookmark(app);
    g_free(line);
    return FALSE;
}

static void start_account_request(App *app, const char *operation, const char *id, const char *credentials) {
    if (app->account_watch) return;
    char *argv[] = {(char *)app->backend, (char *)app->cache, (char *)operation, (char *)id, 0};
    GPid pid;
    gint output, input;
    if (!g_spawn_async_with_pipes(0, argv, 0,
        G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_STDERR_TO_DEV_NULL,
        0, 0, &pid, credentials ? &input : 0, &output, 0, 0)) {
        say(app, "Couldn’t start the Hacker News request.");
        return;
    }
    g_child_watch_add(pid, reap_child, 0);
    if (credentials) {
        GIOChannel *pipe = g_io_channel_unix_new(input);
        g_io_channel_set_close_on_unref(pipe, TRUE);
        g_io_channel_set_encoding(pipe, 0, 0);
        g_io_channel_write_chars(pipe, credentials, -1, 0, 0);
        g_io_channel_shutdown(pipe, TRUE, 0);
        g_io_channel_unref(pipe);
    }
    GIOChannel *pipe = g_io_channel_unix_new(output);
    g_io_channel_set_close_on_unref(pipe, TRUE);
    g_io_channel_set_flags(pipe, G_IO_FLAG_NONBLOCK, 0);
    app->account_story = g_str_equal(operation, "favorite") ? app->story : 0;
    app->account_watch = g_io_add_watch_full(pipe, G_PRIORITY_DEFAULT_IDLE,
        G_IO_IN | G_IO_HUP | G_IO_ERR, read_account_reply, app, 0);
    g_io_channel_unref(pipe);
    if (app->screen == OPTIONS && app->login_button) gtk_widget_set_sensitive(app->login_button, FALSE);
    say(app, credentials ? "Signing in…" : app->account_story ? "Adding to Favorites…" : "Signing out…");
}

static void log_in(GtkWidget *widget, App *app) {
    (void)widget;
    if (app->account_watch) return;
    const char *user = gtk_entry_get_text(GTK_ENTRY(app->username_entry));
    const char *password = gtk_entry_get_text(GTK_ENTRY(app->password_entry));
    if (!*user || !*password) {
        say(app, "Enter your Hacker News username and password.");
        return;
    }
    char *input = g_strdup_printf("%s\n%s\n", user, password);
    gtk_entry_set_text(GTK_ENTRY(app->password_entry), "");
    gtk_widget_hide(app->keyboard);
    start_account_request(app, "login", 0, input);
    for (volatile char *p = input; *p; ++p) *p = 0;
    g_free(input);
}

static void log_out(GtkWidget *widget, App *app) {
    (void)widget;
    start_account_request(app, "logout", 0, 0);
}

/* Runs as an idle after GTK has resized Settings, so the end of the page is really the end. */
static gboolean settings_to_end(gpointer data) {
    App *app = data;
    if (app->screen == OPTIONS && app->content) {
        GtkAdjustment *scroll = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(app->content));
        gtk_adjustment_set_value(scroll, gtk_adjustment_get_upper(scroll) - gtk_adjustment_get_page_size(scroll));
    }
    return FALSE;
}

#ifdef __linux__
enum { KEYBOARD_SHIFT = 256, KEYBOARD_SYMBOLS, KEYBOARD_DONE, KEYBOARD_BACKSPACE };

static void build_keyboard(App *app);

static void keyboard_key(GtkWidget *widget, App *app) {
    int key = value_of(widget);
    if (key == KEYBOARD_DONE) {
        gtk_widget_hide(app->keyboard);
        return;
    }
    if (key == KEYBOARD_SHIFT || key == KEYBOARD_SYMBOLS) {
        app->keyboard_mode = key == KEYBOARD_SHIFT ? (app->keyboard_mode == 1 ? 0 : 1) :
            (app->keyboard_mode == 2 ? 0 : 2);
        build_keyboard(app);
        return;
    }

    GtkWidget *entry = app->keyboard_entry;
    int start, end;
    int position = gtk_editable_get_position(GTK_EDITABLE(entry));
    if (gtk_editable_get_selection_bounds(GTK_EDITABLE(entry), &start, &end)) {
        gtk_editable_delete_text(GTK_EDITABLE(entry), start, end);
        position = start;
    } else if (key == KEYBOARD_BACKSPACE && position > 0) {
        gtk_editable_delete_text(GTK_EDITABLE(entry), position - 1, position);
        --position;
    }
    if (key != KEYBOARD_BACKSPACE) {
        char letter = key;
        gtk_editable_insert_text(GTK_EDITABLE(entry), &letter, 1, &position);
    }
    gtk_editable_set_position(GTK_EDITABLE(entry), position);
}

static void keyboard_button(App *app, GtkWidget *row, const char *text, int key) {
    GtkWidget *control = button(text, G_CALLBACK(keyboard_key), app, key);
    gtk_button_set_focus_on_click(GTK_BUTTON(control), FALSE);
    gtk_box_pack_start(GTK_BOX(row), control, TRUE, TRUE, 0);
}

static void build_keyboard(App *app) {
    static const char *rows[][4] = {
        {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm"},
        {"1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"},
        {"!@#$%^&*()", "-_=+[]{}'\x22", "\x60~:;,.<>/?", "\\|"},
    };
    if (app->keyboard_keys) gtk_widget_destroy(app->keyboard_keys);
    app->keyboard_keys = gtk_vbox_new(FALSE, 1);
    gtk_box_pack_start(GTK_BOX(app->keyboard), app->keyboard_keys, FALSE, TRUE, 0);
    for (int i = 0; i < 4; ++i) {
        GtkWidget *row = gtk_hbox_new(TRUE, 1);
        for (const char *p = rows[app->keyboard_mode][i]; *p; ++p) {
            char text[] = {*p, 0};
            keyboard_button(app, row, text, *p);
        }
        gtk_box_pack_start(GTK_BOX(app->keyboard_keys), row, FALSE, TRUE, 0);
    }
    GtkWidget *row = gtk_hbox_new(TRUE, 1);
    keyboard_button(app, row, "Shift", KEYBOARD_SHIFT);
    keyboard_button(app, row, app->keyboard_mode == 2 ? "ABC" : "Symbols", KEYBOARD_SYMBOLS);
    keyboard_button(app, row, "Space", ' ');
    keyboard_button(app, row, "Delete", KEYBOARD_BACKSPACE);
    keyboard_button(app, row, "Done", KEYBOARD_DONE);
    gtk_box_pack_start(GTK_BOX(app->keyboard_keys), row, FALSE, TRUE, 0);
    gtk_widget_show_all(app->keyboard_keys);
}

static gboolean show_account_keyboard(GtkWidget *entry, void *event, App *app) {
    (void)event;
    app->keyboard_entry = entry;
    if (!app->keyboard_keys) build_keyboard(app);
    gtk_widget_show(app->keyboard);
    g_idle_add(settings_to_end, app);
    return FALSE;
}
#endif

static GtkWidget *account_entry(App *app, const char *name, gboolean password) {
    GtkWidget *row = gtk_hbox_new(FALSE, 8);
    GtkWidget *caption = label(name, 0);
    gtk_widget_set_size_request(caption, app->tap_size * 2, -1);
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 16);
    gtk_entry_set_max_length(GTK_ENTRY(entry), password ? 512 : 64);
    gtk_entry_set_visibility(GTK_ENTRY(entry), !password);
    gtk_widget_set_size_request(entry, -1, app->tap_size);
    action_name(entry, name);
    gtk_box_pack_start(GTK_BOX(row), caption, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(app->content), row, FALSE, TRUE, 0);
    g_signal_connect(entry, "activate", G_CALLBACK(log_in), app);
#ifdef __linux__
    g_signal_connect(entry, "button-press-event", G_CALLBACK(show_account_keyboard), app);
#endif
    return entry;
}

static void render_account(App *app) {
    GtkWidget *row = gtk_hbox_new(FALSE, 8);
    app->username_entry = app->password_entry = app->keyboard_entry = app->keyboard_keys = 0;
    app->keyboard_mode = 0;
    if (app->username) {
        char *identity = g_strdup_printf("Signed in as %s", app->username);
        gtk_box_pack_start(GTK_BOX(row), label(identity, 0), TRUE, TRUE, 0);
        g_free(identity);
        app->login_button = button("Log out", G_CALLBACK(log_out), app, 0);
    } else {
        message(app, "Optional. Signed in, saved stories are also added to your Hacker News Favorites.");
        app->username_entry = account_entry(app, "Username", FALSE);
        app->password_entry = account_entry(app, "Password", TRUE);
        app->login_button = button("Log in", G_CALLBACK(log_in), app, 0);
    }
    gtk_widget_set_sensitive(app->login_button, !app->account_watch);
    gtk_box_pack_end(GTK_BOX(row), app->login_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(app->content), row, FALSE, TRUE, 0);
    app->keyboard = gtk_vbox_new(FALSE, 0);
    gtk_widget_set_no_show_all(app->keyboard, TRUE);
    gtk_box_pack_start(GTK_BOX(app->content), app->keyboard, FALSE, TRUE, 0);
}
