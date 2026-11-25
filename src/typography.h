static void load_typography(App *app) {
    int size = SYSTEM_SIZE;
    char *path = g_build_filename(app->cache, "text-size", (void *)0);
    char *setting = 0;
    if (g_file_get_contents(path, &setting, 0, 0)) {
        int old = number(setting);
        if (old >= 0 && old < 4) size = 1 + old * 2;
    } else {
        g_free(path);
        path = g_build_filename(app->cache, "density", (void *)0);
        if (g_file_get_contents(path, &setting, 0, 0)) {
            if (number(setting) == 0) size = 5;
            if (number(setting) == 2) size = 1;
        }
    }
    g_free(setting);
    g_free(path);
    app->typography = (Typography){0, 0, {size, size, size}};

    path = g_build_filename(app->cache, "typography", (void *)0);
    setting = 0;
    if (g_file_get_contents(path, &setting, 0, 0)) {
        char **parts = g_strsplit(setting, " ", 0);
        if (g_strv_length(parts) == 5) {
            int family = number(parts[0]);
            int bold = number(parts[1]);
            if (family >= 0 && family < FONT_COUNT) app->typography.family = family;
            if (bold >= 0 && bold < BOLD_LEVELS) app->typography.bold = bold;
            for (int area = FEED; area <= ARTICLE; ++area) {
                int level = number(parts[area + 2]);
                if (level >= 0 && level < TEXT_SIZE_COUNT) app->typography.size[area] = level;
            }
        }
        g_strfreev(parts);
    }
    g_free(setting);
    g_free(path);
}

static void find_font_families(App *app) {
    app->font_families = 15u;
    PangoFontFamily **families = 0;
    int count = 0;
    pango_context_list_families(gtk_widget_get_pango_context(app->window), &families, &count);
    for (int family = 4; family < FONT_COUNT; ++family) {
        for (int i = 0; i < count; ++i) {
            if (g_ascii_strcasecmp(font_families[family], pango_font_family_get_name(families[i])) == 0) {
                app->font_families |= 1u << family;
                break;
            }
        }
    }
    g_free(families);
    if (!(app->font_families & (1u << app->typography.family))) app->typography.family = 0;
}

static gboolean save_typography(App *app, Typography next) {
    char *path = g_build_filename(app->cache, "typography", (void *)0);
    char *setting = g_strdup_printf("%d %d %d %d %d", next.family, next.bold,
        next.size[FEED], next.size[COMMENTS], next.size[ARTICLE]);
    gboolean saved = g_file_set_contents(path, setting, -1, 0);
    g_free(setting);
    g_free(path);
    say(app, saved ? "" : "Couldn’t save typography settings.");
    if (saved) {
        app->typography = next;
        app->feed_row_height = 0;
    }
    return saved;
}

enum { BOLD_CONTROL = 3 };

static void update_typography_controls(App *app) {
    for (int row = FEED; row <= BOLD_CONTROL; ++row) {
        int level = row == BOLD_CONTROL ? app->typography.bold : app->typography.size[row];
        int count = row == BOLD_CONTROL ? BOLD_LEVELS : TEXT_SIZE_COUNT;
        int first = row == BOLD_CONTROL ? 0 : 1;
        char *value = g_strdup_printf("%d / %d", level + first, count - 1 + first);
        gtk_label_set_text(GTK_LABEL(app->type_level[row]), value);
        g_free(value);
        gtk_widget_set_sensitive(app->type_down[row], level > 0);
        gtk_widget_set_sensitive(app->type_up[row], level < count - 1);
    }
}

static void update_typography_preview(App *app) {
    static const char *samples[] = {"A story worth reading", "What do you think?", "A quiet moment to read."};
    Screen area = app->preview_area;
    gtk_label_set_text(GTK_LABEL(app->typography_preview), samples[area]);
    reader_font(app, app->typography_preview, reader_scale(app, area), FALSE);
}

static void adjust_typography(GtkWidget *widget, App *app) {
    int control = value_of(widget);
    int row = control / 2;
    int delta = control % 2 ? 1 : -1;
    Typography next = app->typography;
    if (row == BOLD_CONTROL) next.bold += delta;
    else next.size[row] += delta;
    if (!save_typography(app, next)) return;
    if (row != BOLD_CONTROL) app->preview_area = (Screen)row;
    update_typography_preview(app);
    update_typography_controls(app);
}

static void stepper(App *app, int row, const char *title) {
    GtkWidget *line = gtk_hbox_new(FALSE, 8);
    GtkWidget *caption = label(title, 0);
    char *down_name = g_strdup_printf("Decrease %s", title);
    char *up_name = g_strdup_printf("Increase %s", title);
    GtkWidget *down = action(ICON_MINUS, down_name, G_CALLBACK(adjust_typography), app, row * 2);
    GtkWidget *up = action(ICON_PLUS, up_name, G_CALLBACK(adjust_typography), app, row * 2 + 1);
    g_free(down_name);
    g_free(up_name);
    GtkWidget *indicator = gtk_label_new("");
    app->type_level[row] = indicator;
    app->type_down[row] = down;
    app->type_up[row] = up;
    gtk_widget_set_size_request(indicator, app->tap_size, -1);
    gtk_box_pack_start(GTK_BOX(line), caption, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(line), down, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(line), indicator, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(line), up, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(app->content), line, FALSE, TRUE, 0);
}

static void choose_font_family(GtkWidget *widget, App *app) {
    int selected = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
    int active = 0, family = 0, index = 0;
    for (int n = 0; n < FONT_COUNT; ++n) {
        if (!(app->font_families & (1u << n))) continue;
        if (index == selected) family = n;
        if (n == app->typography.family) active = index;
        ++index;
    }
    if (selected < 0 || family == app->typography.family) return;
    Typography next = app->typography;
    next.family = family;
    if (save_typography(app, next)) update_typography_preview(app);
    else gtk_combo_box_set_active(GTK_COMBO_BOX(widget), active);
}

static void render_typography(App *app) {
    app->typography_preview = label("", 0);
    update_typography_preview(app);
    gtk_label_set_ellipsize(GTK_LABEL(app->typography_preview), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(app->content), app->typography_preview, FALSE, TRUE, 8);
    gtk_box_pack_start(GTK_BOX(app->content), gtk_hseparator_new(), FALSE, TRUE, 0);

    GtkWidget *line = gtk_hbox_new(FALSE, 8);
    GtkWidget *picker = new_picker();
    int active = 0, index = 0;
    for (int n = 0; n < FONT_COUNT; ++n) {
        if (!(app->font_families & (1u << n))) continue;
        picker_append(picker, font_families[n]);
        if (n == app->typography.family) active = index;
        ++index;
    }
    action_name(picker, "Font family");
    gtk_combo_box_set_active(GTK_COMBO_BOX(picker), active);
    g_signal_connect(picker, "changed", G_CALLBACK(choose_font_family), app);
    gtk_widget_set_size_request(picker, -1, app->tap_size);
    gtk_box_pack_start(GTK_BOX(line), label("Font family", 0), TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(line), picker, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(app->content), line, FALSE, TRUE, 0);

    stepper(app, BOLD_CONTROL, "Boldness");
    gtk_box_pack_start(GTK_BOX(app->content), gtk_hseparator_new(), FALSE, TRUE, 0);
    for (int n = FEED; n <= ARTICLE; ++n)
        stepper(app, n, size_labels[n]);
    update_typography_controls(app);
}
