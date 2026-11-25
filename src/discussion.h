/* Only the viewport and a small buffer on either side have GTK widgets. */
enum { COMMENT_BUFFER = 10 };

static GtkWidget *comment_text(App *app, const char *text, int width) {
    GtkWidget *view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(view), 0);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(view), 2);
    PangoFontDescription *font = content_font(app, reader_scale(app, COMMENTS), FALSE);
    gtk_widget_modify_font(view, font);
    PangoLayout *layout = gtk_widget_create_pango_layout(view, text);
    pango_layout_set_font_description(layout, font);
    pango_layout_set_width(layout, (width > 6 ? width - 6 : 1) * PANGO_SCALE);
    pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
    int height = 0;
    pango_layout_get_pixel_size(layout, 0, &height);
    g_object_unref(layout);
    pango_font_description_free(font);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(view)), text, -1);
    gtk_widget_set_size_request(view, -1, height + 6);
    return view;
}

static void link_siblings(App *app) {
    CommentRow *last[INLINE_COMMENT_DEPTH + 1] = {0};
    for (int i = 0; i < app->comment_count; ++i) {
        CommentRow *row = &app->comment_rows[i];
        int depth = row->depth;
        if (depth < 0 || depth > INLINE_COMMENT_DEPTH) continue;
        for (int level = depth + 1; level <= INLINE_COMMENT_DEPTH; ++level) last[level] = 0;
        if (depth && last[depth]) {
            row->previous = last[depth]->id;
            last[depth]->next = row->id;
        }
        last[depth] = row;
    }
}

static int comment_tap_size(App *app) {
    int tap = app->tap_size * 2 / 3;
    return tap < 44 ? 44 : tap;
}

static GtkWidget *comment_action(App *app, Icon icon, const char *name, GCallback callback, int id) {
    GtkWidget *control = action(icon, name, callback, app, id);
    int tap = comment_tap_size(app);
    int canvas = app->icon_size * 3 / 4;
    GtkWidget *image = gtk_bin_get_child(GTK_BIN(control));
    gtk_widget_set_size_request(image, canvas, canvas);
    g_object_set_data(G_OBJECT(image), "hn-icon-size", GINT_TO_POINTER(canvas));
    gtk_widget_set_size_request(control, tap, tap);
    return control;
}

static GtkWidget *make_comment_row(App *app, int index, int width) {
    CommentRow *item = &app->comment_rows[index];
    GtkWidget *content = gtk_vbox_new(FALSE, 4);
    if (!item->wire) {
        gtk_box_pack_start(GTK_BOX(content), comment_text(app, app->discussion_text, width), FALSE, TRUE, 3);
        return content;
    }

    char **parts = g_strsplit(item->wire, "\t", -1);
    guint columns = g_strv_length(parts);
    if (columns < 7) {
        g_strfreev(parts);
        return content;
    }
    char *fields[6];
    for (int i = 0; i < 6; ++i) fields[i] = decode(parts[i + 1]);
    int depth = item->depth;
    /* Rules go between top-level comments. The header already has one above the first row. */
    if (depth <= 1 && index > 0) gtk_box_pack_start(GTK_BOX(content), gtk_hseparator_new(), FALSE, FALSE, 0);

    int gap = app->icon_size / 3;
    int text_width = width;
    GtkWidget *row = gtk_hbox_new(FALSE, gap);
    for (int level = 1; level < depth && level < INLINE_COMMENT_DEPTH; ++level) {
        gtk_box_pack_start(GTK_BOX(row), gtk_vseparator_new(), FALSE, TRUE, 0);
        text_width -= gap + 2;
    }
    if (text_width < 1) text_width = 1;
    gboolean collapsed = item->collapsed;
    GtkWidget *copy = gtk_vbox_new(FALSE, 4);
    if (!item->placeholder_text) {
        GtkWidget *byline = gtk_hbox_new(FALSE, 3);
        char *author = g_strdup_printf("%s%s%s", *fields[1] ? fields[1] : "Comment",
            *fields[3] ? "  ·  " : "", fields[3]);
        GtkWidget *heading = label(author, "<span size='small' foreground='#333333'>%s</span>");
        gtk_widget_set_tooltip_text(heading, author);
        g_free(author);
        reader_font(app, heading, text_scales[app->typography.size[COMMENTS]], TRUE);
        int label_width = 0;
        pango_layout_get_pixel_size(gtk_label_get_layout(GTK_LABEL(heading)), &label_width, 0);
        int previous = item->previous;
        int next = item->next;
        int reserved = (1 + (previous != 0) + (next != 0)) * (comment_tap_size(app) + 16);
        int available = text_width - reserved;
        if (available < 1) available = 1;
        gtk_widget_set_size_request(heading, label_width < available ? label_width : available, -1);
        gtk_label_set_ellipsize(GTK_LABEL(heading), PANGO_ELLIPSIZE_END);
        gtk_box_pack_start(GTK_BOX(byline), heading, FALSE, TRUE, 0);
        if (previous)
            gtk_box_pack_start(GTK_BOX(byline), comment_action(app, ICON_UP, "Previous comment",
                G_CALLBACK(jump_comment), previous), FALSE, FALSE, 0);
        if (next)
            gtk_box_pack_start(GTK_BOX(byline), comment_action(app, ICON_DOWN, "Next comment",
                G_CALLBACK(jump_comment), next), FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(byline), comment_action(app, collapsed ? ICON_PLUS : ICON_MINUS,
            collapsed ? "Expand comment" : "Collapse comment", G_CALLBACK(toggle_comment), item->id), FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(copy), byline, FALSE, TRUE, 0);
    }
    if (!collapsed) {
        const char *body = item->placeholder_text ? item->placeholder_text : fields[2];
        gtk_box_pack_start(GTK_BOX(copy), comment_text(app, body, text_width), FALSE, TRUE, 0);
        char *url_text = decode(columns >= 8 ? parts[7] : 0);
        char **links = g_strsplit(url_text, "\n", -1);
        for (int i = 0; links[i]; ++i) {
            GtkWidget *link = button("", G_CALLBACK(comment_link_clicked), app, 0);
            gtk_widget_destroy(gtk_bin_get_child(GTK_BIN(link)));
            GtkWidget *caption = label(links[i], "<u>%s</u>");
            reader_font(app, caption, text_scales[app->typography.size[COMMENTS]], FALSE);
            gtk_label_set_ellipsize(GTK_LABEL(caption), PANGO_ELLIPSIZE_END);
            gtk_container_add(GTK_CONTAINER(link), caption);
            action_name(link, links[i]);
            g_object_set_data_full(G_OBJECT(link), "hn-url", g_strdup(links[i]), g_free);
            gtk_box_pack_start(GTK_BOX(copy), link, FALSE, TRUE, 0);
        }
        g_strfreev(links);
        g_free(url_text);
        int children = number(fields[4]);
        if (depth == INLINE_COMMENT_DEPTH && children > 0)
            thread_link(copy, app, item->id, children);
    }
    gtk_box_pack_start(GTK_BOX(row), copy, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(content), row, FALSE, TRUE, 4);
    for (int i = 0; i < 6; ++i) g_free(fields[i]);
    g_strfreev(parts);
    return content;
}

static void clear_comment_window(App *app) {
    if (app->comment_idle) g_source_remove(app->comment_idle);
    app->comment_idle = 0;
    app->comment_layout = 0;
    app->comment_scroll = 0;
    g_free(app->comment_rows);
    g_strfreev(app->comment_lines);
    app->comment_rows = 0;
    app->comment_lines = 0;
    app->comment_count = 0;
    app->comment_width = 0;
    app->comment_geometry_dirty = FALSE;
}

static int position_comments(App *app) {
    int top = LIST_INSET;
    for (int i = 0; i < app->comment_count; i++) {
        CommentRow *item = &app->comment_rows[i];
        item->top = top;
        top += item->height;
    }
    return top + LIST_INSET;
}

static int first_comment_at(App *app, double offset) {
    int i = 0;
    while (i + 1 < app->comment_count &&
        app->comment_rows[i].top + app->comment_rows[i].height <= offset) ++i;
    return i;
}

static void remember_comment_position(App *app) {
    if (!app->comment_scroll || !app->comment_count || app->comment_restore) return;
    double offset = gtk_adjustment_get_value(app->comment_scroll);
    int index = first_comment_at(app, offset);
    app->comment_anchor[app->depth] = app->comment_rows[index].id;
    app->comment_offset[app->depth] = offset - app->comment_rows[index].top;
}

static void jump_comment(GtkWidget *widget, App *app) {
    int id = value_of(widget);
    for (int i = 0; i < app->comment_count; ++i) {
        if (app->comment_rows[i].id == id) {
            gtk_adjustment_set_value(app->comment_scroll, app->comment_rows[i].top);
            return;
        }
    }
}

static void schedule_comments(App *app);

static gboolean update_comment_window(gpointer data) {
    App *app = data;
    app->comment_idle = 0;
    if (app->screen != COMMENTS || !app->comment_layout || !app->comment_count) return FALSE;
    GtkAllocation bounds;
    gtk_widget_get_allocation(app->comment_layout, &bounds);
    int width = bounds.width - 2 * LIST_INSET;
    double viewport = gtk_adjustment_get_page_size(app->comment_scroll);
    if (width < 1 || viewport < 2) return FALSE;

    double offset = gtk_adjustment_get_value(app->comment_scroll);
    if (app->comment_restore) {
        int id = app->comment_anchor[app->depth];
        offset = 0;
        if (id) {
            for (int i = 0; i < app->comment_count; ++i) {
                const CommentRow *item = &app->comment_rows[i];
                if (item->id == id) {
                    double intra = app->comment_offset[app->depth];
                    if (intra >= item->height) intra = item->height - 1;
                    offset = item->top + intra;
                    break;
                }
            }
        }
        app->comment_restore = FALSE;
    }
    int anchor = first_comment_at(app, offset);
    double within = offset - app->comment_rows[anchor].top;
    gboolean resized = app->comment_width != width;
    if (resized) {
        int estimate = app->tap_size + app->icon_size;
        for (int i = 0; i < app->comment_count; ++i) {
            CommentRow *item = &app->comment_rows[i];
            if (item->widget) gtk_widget_destroy(item->widget);
            item->widget = 0;
            item->height = estimate;
        }
        app->comment_width = width;
        position_comments(app);
        offset = app->comment_rows[anchor].top + within;
    }
    int start = first_comment_at(app, offset) - COMMENT_BUFFER;
    int end = first_comment_at(app, offset + viewport) + COMMENT_BUFFER + 1;
    if (start < 0) start = 0;
    if (end > app->comment_count) end = app->comment_count;
    gboolean measured = FALSE;
    for (int i = 0; i < app->comment_count; ++i) {
        CommentRow *item = &app->comment_rows[i];
        if (i < start || i >= end) {
            if (item->widget) gtk_widget_destroy(item->widget);
            item->widget = 0;
            continue;
        }
        if (item->widget) continue;
        item->widget = make_comment_row(app, i, width);
        gtk_widget_set_size_request(item->widget, width, -1);
        gtk_layout_put(GTK_LAYOUT(app->comment_layout), item->widget, LIST_INSET, item->top);
        item->mounted_top = item->top;
        gtk_widget_show_all(item->widget);
        int height = 0;
#if defined(GTK_MAJOR_VERSION) && GTK_MAJOR_VERSION >= 3
        gtk_widget_get_preferred_height_for_width(item->widget, width, 0, &height);
#else
        GtkRequisition requested;
        gtk_widget_size_request(item->widget, &requested);
        height = requested.height;
#endif
        if (height > 0 && item->height != height) {
            item->height = height;
            measured = TRUE;
        }
    }

    if (measured || resized || app->comment_geometry_dirty) {
        app->comment_geometry_dirty = FALSE;
        int content_height = position_comments(app);
        guint old_width, old_height;
        gtk_layout_get_size(GTK_LAYOUT(app->comment_layout), &old_width, &old_height);
        if (old_width != (guint)bounds.width || old_height != (guint)content_height)
            gtk_layout_set_size(GTK_LAYOUT(app->comment_layout), bounds.width, content_height);
        for (int i = start; i < end; ++i) {
            CommentRow *item = &app->comment_rows[i];
            if (item->mounted_top != item->top) {
                gtk_layout_move(GTK_LAYOUT(app->comment_layout), item->widget, LIST_INSET, item->top);
                item->mounted_top = item->top;
            }
        }
        offset = app->comment_rows[anchor].top + within;
    }
    gtk_adjustment_set_value(app->comment_scroll, offset);
    update_scroll_controls(app->comment_scroll, app);
    if (measured) schedule_comments(app);
    return FALSE;
}

static void schedule_comments(App *app) {
    if (app->screen == COMMENTS && app->comment_layout && !app->comment_idle)
        app->comment_idle = g_idle_add(update_comment_window, app);
}

static void comments_scrolled(GtkAdjustment *adjustment, App *app) {
    (void)adjustment;
    schedule_comments(app);
}

static void comments_allocated(GtkWidget *widget, GtkAllocation *size, App *app) {
    (void)widget;
    (void)size;
    schedule_comments(app);
}

static void maintain_comment_widgets(App *app, CommentRow *old, int old_count) {
    GHashTable *by_id = g_hash_table_new(0, 0);
    for (int i = 0; i < old_count; ++i) {
        if (old[i].id) g_hash_table_insert(by_id, GINT_TO_POINTER(old[i].id), &old[i]);
    }
    for (int i = 0; i < app->comment_count; ++i) {
        CommentRow *row = &app->comment_rows[i];
        CommentRow *before = g_hash_table_lookup(by_id, GINT_TO_POINTER(row->id));
        if (before && row->placeholder_text == before->placeholder_text &&
            row->collapsed == before->collapsed && row->previous == before->previous &&
            row->next == before->next && g_strcmp0(row->wire, before->wire) == 0) {
            row->height = before->height;
            row->widget = before->widget;
            row->mounted_top = before->mounted_top;
            before->widget = 0;
        } else {
            row->height = app->tap_size + app->icon_size;
        }
    }
    g_hash_table_destroy(by_id);
    for (int i = 0; i < old_count; ++i)
        if (old[i].widget) gtk_widget_destroy(old[i].widget);
}

static void build_discussion_viewport(App *app, int height) {
    if (app->content) gtk_widget_destroy(app->content);
    GtkWidget *scroll = gtk_scrolled_window_new(0, 0);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    GtkWidget *scrollbar = gtk_scrolled_window_get_vscrollbar(GTK_SCROLLED_WINDOW(scroll));
    gtk_widget_set_name(scrollbar, "hnink-discussion-scrollbar");
    int width = app->icon_size / 3;
    gtk_widget_set_size_request(scrollbar, width < 14 ? 14 : width, -1);
    gtk_box_pack_start(GTK_BOX(app->body), scroll, TRUE, TRUE, 0);
    app->content = scroll;
    app->comment_layout = gtk_layout_new(0, 0);
    gtk_widget_set_name(app->comment_layout, "hnink-paper");
    gtk_container_add(GTK_CONTAINER(scroll), app->comment_layout);
    gtk_layout_set_size(GTK_LAYOUT(app->comment_layout), 1, height);
    app->comment_scroll = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroll));
    g_signal_connect(app->comment_scroll, "value-changed", G_CALLBACK(comments_scrolled), app);
    g_signal_connect(app->comment_scroll, "changed", G_CALLBACK(comments_scrolled), app);
    g_signal_connect(app->comment_layout, "size-allocate", G_CALLBACK(comments_allocated), app);
    gtk_widget_show_all(scroll);
    show_scroll_controls(app, app->comment_scroll);
}

static void mount_discussion(App *app, const char *output) {
    char **old_lines = app->comment_lines;
    CommentRow *old_rows = app->comment_rows;
    int old_count = app->comment_count;
    app->comment_lines = g_strsplit(output, "\n", -1);
    app->comment_rows = g_malloc0((g_strv_length(app->comment_lines) + 1) * sizeof(CommentRow));
    app->comment_count = 0;
    if (!app->depth && app->discussion_text && *app->discussion_text)
        app->comment_rows[app->comment_count++].id = -1;
    gboolean has_comments = FALSE;
    int hidden_below = -1;
    for (int n = 0; app->comment_lines[n]; n++) {
        const char *line = app->comment_lines[n];
        if (!g_str_has_prefix(line, "comment\t") && !g_str_has_prefix(line, "missing\t") &&
            !g_str_has_prefix(line, "context\t")) continue;
        if (!g_str_has_prefix(line, "context\t")) has_comments = TRUE;
        char **parts = g_strsplit(line, "\t", -1);
        if (g_strv_length(parts) >= 7) {
            char *depth_text = decode(parts[6]);
            int depth = number(depth_text);
            g_free(depth_text);
            if (hidden_below < 0 || depth <= hidden_below) {
                hidden_below = -1;
                char *id_text = decode(parts[1]);
                int id = number(id_text);
                g_free(id_text);
                CommentRow *row = &app->comment_rows[app->comment_count++];
                row->wire = line;
                if (g_str_equal(parts[0], "missing"))
                    row->placeholder_text = app->discussion_watch ? loading_comment : "Couldn’t load this comment.";
                row->id = id;
                row->depth = depth;
                row->collapsed = g_hash_table_lookup(app->collapsed, GINT_TO_POINTER(id)) != 0;
                if (row->collapsed) hidden_below = depth;
            }
        }
        g_strfreev(parts);
    }

    link_siblings(app);
    maintain_comment_widgets(app, old_rows, old_count);
    g_free(old_rows);
    g_strfreev(old_lines);
    int content_height = position_comments(app);
    if (!has_comments)
        say(app, *output ? "No comments yet." : "Discussion unavailable. Reopen it to try again.");

    app->comment_restore = TRUE;
    if (!app->comment_layout) {
        build_discussion_viewport(app, content_height);
        schedule_comments(app);
        return;
    }
    if (!app->comment_count) {
        gtk_layout_set_size(GTK_LAYOUT(app->comment_layout), 1, content_height);
        update_scroll_controls(app->comment_scroll, app);
        return;
    }
    app->comment_geometry_dirty = TRUE;
    if (app->comment_idle) {
        g_source_remove(app->comment_idle);
        app->comment_idle = 0;
    }
    update_comment_window(app);
}
