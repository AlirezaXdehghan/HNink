#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#ifdef HNREADER_ARM_IMPORTS
#include "gtk2-imports.h"
#else
#include <gtk/gtk.h>
#endif
#include "icons.h"

typedef enum { FEED, COMMENTS, ARTICLE, OPTIONS } Screen;
enum { BOLD_LEVELS = 5 };
typedef struct {
    int family, bold;
    int size[3];
} Typography;
enum { RETURN_TO_KINDLE_HOME = 10 };
typedef enum { SYNC, HYDRATE, LOAD_ARTICLE } Operation;

typedef struct {
    Operation operation;
    int revision, feed, id, page, size, anchor;
    gboolean fresh;
} Task;

typedef struct {
    const char *wire;
    const char *placeholder_text;
    GtkWidget *widget;
    int top, mounted_top, height, id, depth, previous, next;
    gboolean collapsed;
} CommentRow;

typedef struct {
    const char *backend, *cache;
    GtkWidget *window, *header, *feed_picker, *status, *body, *content;
    GtkWidget *left, *refresh, *options, *heading, *discussion_globe, *discussion_book;
    GtkWidget *discussion_save, *scroll_up, *footer, *previous, *next, *page_label;
    GtkWidget *heading_divider, *rail_divider, *status_rail;
    GtkAdjustment *reading_position;
    GtkWidget *username_entry, *password_entry, *login_button, *keyboard, *keyboard_keys, *keyboard_entry;
    int keyboard_mode, account_story;
    guint account_watch;
    char *username;
    gboolean discussion_favorite;
    Screen screen;
    int revision, feed, page, pages, first_id, story, depth;
    int scroll_divisor;
    Typography typography;
    unsigned int font_families;
    Screen preview_area;
    GtkWidget *typography_preview, *type_level[4], *type_down[4], *type_up[4];
    int comment_id[32], comment_anchor[32];
    guint discussion_watch, page_buttons_watch;
    gboolean reverse_page_buttons;
#ifdef __linux__
    guint button_orientation_watch;
    int button_revision;
    gboolean buttons_reversed;
    GString *button_steps;
#endif
    GHashTable *collapsed;
    GtkAdjustment *comment_scroll;
    GtkWidget *comment_layout;
    CommentRow *comment_rows;
    char **comment_lines;
    int comment_count, comment_width;
    guint comment_idle;
    gboolean comment_restore, comment_geometry_dirty;
    double comment_offset[32];
    gboolean discussion_saved;
    char *browser_url;
    int exit_status;
    int feed_size, feed_row_height, feed_body_height;
    int tap_size, icon_size, headline_height, metadata_height, rank_width;
    PangoFontDescription *ui_font;
    guint feed_layout_idle;
    gboolean ready;
    char *last_output, *notice, *article_url, *discussion_text, *discussion_title, *updated;
    gboolean active, queued;
    Task pending;
} App;

static const char *feed_keys[] = {"top", "new", "best", "ask", "show", "jobs", "saved"};
static const char *feed_names[] = {"Top", "New", "Best", "Ask HN", "Show HN", "Jobs", "Saved"};
static const int text_scales[] = {74, 82, 91, 100, 112, 125, 140, 155, 175, 195};
static const char *font_families[] = {"System", "Serif", "Sans Serif", "Monospace", "Bookerly", "Amazon Ember", "Caecilia"};
static const char *size_labels[] = {"Feed size", "Comments size", "Article size"};
enum { FONT_COUNT = sizeof(font_families) / sizeof(font_families[0]), SYSTEM_SIZE = 3 };
static const char *scroll_distances[] = {"1 page", "1/2", "1/3", "1/4"};
static const char loading_comment[] = "Loading the comment…";
enum { TEXT_SIZE_COUNT = sizeof(text_scales) / sizeof(text_scales[0]), FEED_MAX_STORIES = 60, LIST_INSET = 4 };
enum { SCROLL_DISTANCE_COUNT = sizeof(scroll_distances) / sizeof(scroll_distances[0]) };
enum { INLINE_COMMENT_DEPTH = 3 };

static void render(App *app, int anchor);
static void request(App *app, Task task);
static void refresh_feed(App *app, gboolean manual);
static void load_article(GtkWidget *widget, App *app);
static void bookmark(GtkWidget *widget, App *app);
static void update_bookmark(App *app);
static void open_replies(GtkWidget *widget, App *app);

static void open_publisher(GtkWidget *widget, App *app);
static void clear_comment_window(App *app);
static void stop_discussion_read(App *app);
static void comment_link_clicked(GtkWidget *, App *);
static void toggle_comment(GtkWidget *, App *);
static void jump_comment(GtkWidget *, App *);

static char *decode(const char *field) {
    if (!field || !*field) return g_strdup("");
    gsize length = 0;
    guchar *raw = g_base64_decode(field, &length);
    char *text = g_strndup((const char *)raw, length);
    g_free(raw);
    if (!g_utf8_validate(text, -1, 0)) {
        g_free(text);
        return g_strdup("");
    }
    return text;
}

static int number(const char *text) {
    return (int)g_ascii_strtoll(text, 0, 10);
}

static void say(App *app, const char *notice) {
    g_free(app->notice);
    app->notice = g_strdup(notice);
    const char *visible = notice && *notice ? notice : app->screen == FEED ? app->updated : 0;
    gtk_label_set_text(GTK_LABEL(app->status), visible ? visible : "");
    if (app->screen == FEED || (visible && *visible)) gtk_widget_show(app->status_rail);
    else gtk_widget_hide(app->status_rail);
}

static char *local_command(App *app, const char *command, const char *arg, int page, int size, int anchor) {
    char *page_arg = g_strdup_printf("%d", page);
    char *size_arg = g_strdup_printf("%d", size);
    char *anchor_arg = g_strdup_printf("%d", anchor);
    char *argv[] = {(char *)app->backend, (char *)app->cache, (char *)command,
                    (char *)arg, page_arg, size_arg, anchor_arg, 0};
    char *output = 0;
    int status = -1;
    gboolean started = g_spawn_sync(0, argv, 0, G_SPAWN_STDERR_TO_DEV_NULL, 0, 0, &output, 0, &status, 0);
    g_free(page_arg);
    g_free(size_arg);
    g_free(anchor_arg);
    if (!started || status != 0) {
        g_free(output);
        return 0;
    }
    return output;
}

static void reset_output(App *app) {
    g_free(app->last_output);
    app->last_output = 0;
}

static void select_screen(App *app, Screen screen) {
    int anchor = screen == FEED && app->screen == OPTIONS ? app->first_id : 0;
    app->reading_position = 0;
    stop_discussion_read(app);
    clear_comment_window(app);
    app->screen = screen;
    app->revision++;
    reset_output(app);
    say(app, "");
    render(app, anchor);
}

static GtkWidget *label(const char *text, const char *format) {
    GtkWidget *widget = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(widget), 0.0, 0.5);
    if (!format) {
        gtk_label_set_text(GTK_LABEL(widget), text);
    } else {
        char *escaped = g_markup_escape_text(text, -1);
        char *markup = g_strdup_printf(format, escaped);
        gtk_label_set_markup(GTK_LABEL(widget), markup);
        g_free(markup);
        g_free(escaped);
    }
    return widget;
}

static PangoFontDescription *scaled_font(const PangoFontDescription *base, int scale) {
    PangoFontDescription *font = pango_font_description_copy(base);
    int size = pango_font_description_get_size(base);
    if (size > 0 && scale != 100) {
        int scaled = size * scale / 100;
        if (pango_font_description_get_size_is_absolute(base))
            pango_font_description_set_absolute_size(font, scaled);
        else
            pango_font_description_set_size(font, scaled);
    }
    return font;
}

static PangoFontDescription *content_font(App *app, int scale, gboolean emphasized) {
    PangoFontDescription *font = scaled_font(app->ui_font, scale);
    int family = app->typography.family;
    if (family && (app->font_families & (1u << family)))
        pango_font_description_set_family(font, font_families[family]);
    if (app->typography.bold || emphasized) {
        int weight = emphasized ? 700 + 50 * app->typography.bold : 400 + 100 * app->typography.bold;
        pango_font_description_set_weight(font, weight);
    }
    return font;
}

static int reader_scale(App *app, Screen area) {
    int scale = text_scales[app->typography.size[area]];
    return area == COMMENTS ? scale * 120 / 100 : scale;
}

static void reader_font(App *app, GtkWidget *widget, int scale, gboolean emphasized) {
    PangoFontDescription *font = content_font(app, scale, emphasized);
    gtk_widget_modify_font(widget, font);
    pango_font_description_free(font);
}

static void measure_text(App *app, const PangoFontDescription *font, const char *markup,
    int *width, int *height) {
    PangoLayout *layout = gtk_widget_create_pango_layout(app->window, 0);
    pango_layout_set_font_description(layout, font);
    pango_layout_set_markup(layout, markup, -1);
    pango_layout_get_pixel_size(layout, width, height);
    g_object_unref(layout);
}

static void measure_feed_text(App *app) {
    int scale = reader_scale(app, FEED);
    PangoFontDescription *font = content_font(app, scale, TRUE);
    measure_text(app, font, "<span size='large'>HgjÅ</span>", 0, &app->headline_height);
    pango_font_description_free(font);
    font = content_font(app, scale, FALSE);
    measure_text(app, font, "<span size='small'>HgjÅ</span>", 0, &app->metadata_height);
    measure_text(app, font, "<span size='small'>999</span>", &app->rank_width, 0);
    pango_font_description_free(font);
}

/* Paint the content directly, with a local inversion on press. */
static void invert_control(GtkWidget *widget, cairo_t *cr, int x, int y) {
    GtkAllocation bounds;
    gtk_widget_get_allocation(widget, &bounds);
    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_DIFFERENCE);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_rectangle(cr, x, y, bounds.width, bounds.height);
    cairo_fill(cr);
    cairo_restore(cr);
}

#if defined(GTK_MAJOR_VERSION) && GTK_MAJOR_VERSION >= 3
static gboolean draw_control(GtkWidget *widget, cairo_t *cr, gpointer data) {
    (void)data;
    GtkWidget *child = gtk_bin_get_child(GTK_BIN(widget));
    if (child) gtk_container_propagate_draw(GTK_CONTAINER(widget), child, cr);
    if (g_object_get_data(G_OBJECT(widget), "hn-selected") || g_object_get_data(G_OBJECT(widget), "hn-pressed"))
        invert_control(widget, cr, 0, 0);
    return TRUE;
}
#else
static gboolean expose_control(GtkWidget *widget, GdkEventExpose *event, gpointer data) {
    (void)data;
    GtkWidget *child = gtk_bin_get_child(GTK_BIN(widget));
    if (child) gtk_container_propagate_expose(GTK_CONTAINER(widget), child, event);
    if (g_object_get_data(G_OBJECT(widget), "hn-selected") || g_object_get_data(G_OBJECT(widget), "hn-pressed")) {
        GtkAllocation bounds;
        gtk_widget_get_allocation(widget, &bounds);
        cairo_t *cr = gdk_cairo_create(event->window);
        gdk_cairo_region(cr, event->region);
        cairo_clip(cr);
        invert_control(widget, cr, bounds.x, bounds.y);
        cairo_destroy(cr);
    }
    return TRUE;
}
#endif

static void cancel_release(GtkWidget *widget, gpointer data) {
    (void)data;
    guint timer = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(widget), "hn-release"));
    if (timer) g_source_remove(timer);
    g_object_set_data(G_OBJECT(widget), "hn-release", 0);
}

static gboolean release_control(gpointer data) {
    GtkWidget *widget = data;
    g_object_set_data(G_OBJECT(widget), "hn-release", 0);
    g_object_set_data(G_OBJECT(widget), "hn-pressed", 0);
    gtk_widget_queue_draw(widget);
    return FALSE;
}

static void control_released(GtkWidget *widget, gpointer data) {
    cancel_release(widget, data);
    guint timer = g_timeout_add(95, release_control, widget);
    g_object_set_data(G_OBJECT(widget), "hn-release", GUINT_TO_POINTER(timer));
}

static void control_pressed(GtkWidget *widget, gpointer data) {
    cancel_release(widget, data);
    g_object_set_data(G_OBJECT(widget), "hn-pressed", GINT_TO_POINTER(1));
    gtk_widget_queue_draw(widget);
    void *window = gtk_widget_get_window(widget);
    if (window) gdk_window_process_updates(window, FALSE);
}

static GtkWidget *button(const char *text, GCallback callback, App *app, int value) {
    GtkWidget *widget = gtk_button_new_with_label(text);
    g_object_set_data(G_OBJECT(widget), "hn-value", GINT_TO_POINTER(value));
    if (callback) g_signal_connect(widget, "clicked", callback, app);
    g_signal_connect(widget, "pressed", G_CALLBACK(control_pressed), 0);
    g_signal_connect(widget, "released", G_CALLBACK(control_released), 0);
    g_signal_connect(widget, "destroy", G_CALLBACK(cancel_release), 0);
    gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NONE);
#if defined(GTK_MAJOR_VERSION) && GTK_MAJOR_VERSION >= 3
    g_signal_connect(widget, "draw", G_CALLBACK(draw_control), 0);
#else
    g_signal_connect(widget, "expose-event", G_CALLBACK(expose_control), 0);
#endif
    gtk_widget_set_size_request(widget, -1, app->tap_size);
    return widget;
}

static GtkWidget *action(Icon icon, const char *name, GCallback callback, App *app, int value) {
    GtkWidget *widget = button("", callback, app, value);
    gtk_widget_destroy(gtk_bin_get_child(GTK_BIN(widget)));
    gtk_container_add(GTK_CONTAINER(widget), icon_widget(icon, app->icon_size));
    gtk_widget_set_size_request(widget, app->tap_size, app->tap_size);
    if (name) action_name(widget, name);
    return widget;
}

static int value_of(GtkWidget *widget) {
    return GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "hn-value"));
}

static void clear_content(App *app, gboolean equal_rows) {
    if (app->content) gtk_widget_destroy(app->content);
    app->content = gtk_vbox_new(equal_rows, equal_rows ? 0 : 10);
    gtk_box_pack_start(GTK_BOX(app->body), app->content, TRUE, TRUE, 0);
}

static void wrap(GtkWidget *widget) {
    gtk_label_set_line_wrap(GTK_LABEL(widget), TRUE);
    gtk_label_set_line_wrap_mode(GTK_LABEL(widget), PANGO_WRAP_WORD_CHAR);
}

static void message(App *app, const char *text) {
    GtkWidget *widget = label(text, 0);
    wrap(widget);
    gtk_box_pack_start(GTK_BOX(app->content), widget, FALSE, TRUE, 12);
}

static void header(App *app) {
    gboolean feed = app->screen == FEED;
    gboolean discussion = app->screen == COMMENTS;
    gboolean scrolling = discussion || app->screen == ARTICLE;
    const char *title = app->screen == OPTIONS ? "Settings" : "HNink";
    if (scrolling && app->discussion_title) {
        gtk_label_set_text(GTK_LABEL(app->heading), app->discussion_title);
        gtk_widget_set_tooltip_text(app->heading, app->discussion_title);
    } else {
        char *markup = g_strdup_printf(feed ? "<span size='x-large'>%s</span>"
            : "<span size='x-large' weight='bold'>%s</span>", title);
        gtk_label_set_markup(GTK_LABEL(app->heading), markup);
        g_free(markup);
        gtk_widget_set_tooltip_text(app->heading, 0);
    }
    if (scrolling || feed) reader_font(app, app->heading, 100, TRUE);
    else gtk_widget_modify_font(app->heading, app->ui_font);
    gboolean has_link = discussion && app->article_url && *app->article_url;
    if (has_link) gtk_widget_show(app->discussion_book);
    else gtk_widget_hide(app->discussion_book);
    if (discussion) {
        update_bookmark(app);
        gtk_widget_show(app->discussion_save);
    }
    else gtk_widget_hide(app->discussion_save);
    action_name(app->left, feed ? "Back to Kindle Home" : "Back");
    if (feed) gtk_widget_show(app->refresh);
    else gtk_widget_hide(app->refresh);
    if (has_link) gtk_widget_show(app->discussion_globe);
    else gtk_widget_hide(app->discussion_globe);
    GtkWidget *forward = gtk_bin_get_child(GTK_BIN(app->next));
    g_object_set_data(G_OBJECT(forward), "hn-icon", GINT_TO_POINTER(scrolling ? ICON_DOWN : ICON_NEXT));
    gtk_widget_queue_draw(forward);
    if (scrolling) {
        gtk_widget_set_sensitive(app->scroll_up, FALSE);
        gtk_widget_show(app->scroll_up);
    } else {
        gtk_widget_hide(app->scroll_up);
    }
    if (feed) {
        gtk_widget_hide(app->heading_divider);
        gtk_widget_show(app->rail_divider);
        gtk_widget_show(app->feed_picker);
        gtk_widget_show(app->previous);
    } else {
        gtk_widget_show(app->heading_divider);
        gtk_widget_hide(app->rail_divider);
        gtk_widget_hide(app->feed_picker);
        gtk_widget_hide(app->previous);
    }
    action_name(app->next, scrolling ? "Scroll down" : "Next page");
    if (feed) gtk_widget_show(app->options);
    else gtk_widget_hide(app->options);
}

static void pagination(App *app, int page, int pages, gboolean visible) {
    if (!visible) {
        gtk_widget_hide(app->footer);
        return;
    }
    char *text = g_strdup_printf("%d / %d", page, pages);
    gtk_label_set_text(GTK_LABEL(app->page_label), text);
    g_free(text);
    gtk_widget_set_sensitive(app->previous, page > 1);
    gtk_widget_set_sensitive(app->next, page < pages);
    gtk_widget_show(app->footer);
}

static void update_scroll_controls(GtkAdjustment *scroll, App *app) {
    double offset = gtk_adjustment_get_value(scroll);
    double viewport = gtk_adjustment_get_page_size(scroll);
    double end = gtk_adjustment_get_upper(scroll) - viewport;
    gtk_widget_set_sensitive(app->scroll_up, viewport > 1 && offset > 1);
    gtk_widget_set_sensitive(app->next, viewport > 1 && offset < end - 1);
}

static void show_scroll_controls(App *app, GtkAdjustment *scroll) {
    gtk_label_set_text(GTK_LABEL(app->page_label), "");
    update_scroll_controls(scroll, app);
    gtk_widget_show(app->footer);
}

static GtkWidget *feed_row(App *app, const char *const *fields) {
    int id = number(fields[0]);
    GtkWidget *row = gtk_hbox_new(FALSE, 2);
    GtkWidget *story_button = button("", G_CALLBACK(open_replies), app, id);
    action_name(story_button, fields[2]);
    gtk_widget_destroy(gtk_bin_get_child(GTK_BIN(story_button)));
    GtkWidget *rank = label(fields[1], "<span foreground='#555555' size='small'>%s</span>");
    int scale = reader_scale(app, FEED);
    reader_font(app, rank, scale, FALSE);
    gtk_misc_set_alignment(GTK_MISC(rank), 0.5, 0.5);
    gtk_widget_set_size_request(rank, app->rank_width + 6, app->metadata_height);
    gtk_box_pack_start(GTK_BOX(row), rank, FALSE, TRUE, 0);
    GtkWidget *center = gtk_alignment_new(0, 0.5, 1, 0);
    GtkWidget *copy = gtk_vbox_new(FALSE, 2);
    GtkWidget *title = label(fields[2], "<span size='large'>%s</span>");
    reader_font(app, title, scale, !*fields[4]);
    gtk_widget_set_size_request(title, -1, app->headline_height);
    gtk_label_set_ellipsize(GTK_LABEL(title), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(copy), title, FALSE, TRUE, 0);
    if (*fields[3]) {
        GtkWidget *meta = label(fields[3], "<span foreground='#555555' size='small'>%s</span>");
        reader_font(app, meta, scale, FALSE);
        gtk_widget_set_size_request(meta, -1, app->metadata_height);
        gtk_label_set_ellipsize(GTK_LABEL(meta), PANGO_ELLIPSIZE_END);
        gtk_box_pack_start(GTK_BOX(copy), meta, FALSE, TRUE, 0);
    }
    gtk_container_set_border_width(GTK_CONTAINER(copy), 4);
    gtk_container_add(GTK_CONTAINER(center), copy);
    gtk_box_pack_start(GTK_BOX(row), center, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(story_button), row);
    GtkWidget *entry = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(entry), story_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(entry), gtk_hseparator_new(), FALSE, FALSE, 0);
    return entry;
}

static int measure_feed_row(App *app) {
    measure_feed_text(app);
    const char *sample[] = {"0", "99", "Typography", "example.com · 99 points · 99 comments · 2h", ""};
    GtkWidget *probe = feed_row(app, sample);
    gtk_box_pack_start(GTK_BOX(app->body), probe, FALSE, FALSE, 0);
    gtk_widget_show_all(probe);
    int height;
#if defined(GTK_MAJOR_VERSION) && GTK_MAJOR_VERSION >= 3
    gtk_widget_get_preferred_height(probe, &height, 0);
#else
    GtkRequisition requested;
    gtk_widget_size_request(probe, &requested);
    height = requested.height;
#endif
    gtk_widget_destroy(probe);
    /* Two measured lines, their gap, insets, and room for the GTK theme. */
    int text_height = app->headline_height + app->metadata_height + 16;
    return height > text_height ? height : text_height;
}

static int fit_feed(App *app) {
    if (!app->feed_row_height) app->feed_row_height = measure_feed_row(app);
    int available = app->feed_body_height - 2 * LIST_INSET;
    int count = 1;
    while (count < FEED_MAX_STORIES && (count + 1) * app->feed_row_height <= available) count++;
    return count;
}

static void hydrate_feed_page(App *app) {
    if (app->feed != 6) request(app, (Task){HYDRATE, app->revision, app->feed, 0, app->page, app->feed_size, 0, FALSE});
}

static gboolean reflow_feed(gpointer data) {
    App *app = data;
    app->feed_layout_idle = 0;
    if (app->screen != FEED) return FALSE;
    int count = fit_feed(app);
    if (count != app->feed_size) {
        int anchor = app->first_id;
        app->feed_size = count;
        app->revision++;
        render(app, anchor);
        hydrate_feed_page(app);
    }
    return FALSE;
}

static void feed_body_allocated(GtkWidget *widget, GtkAllocation *bounds, App *app) {
    (void)widget;
    if (app->screen != FEED || app->feed_body_height == bounds->height) return;
    app->feed_body_height = bounds->height;
    if (app->ready && !app->feed_layout_idle) app->feed_layout_idle = g_idle_add(reflow_feed, app);
}

static void thread_link(GtkWidget *list, App *app, int id, int replies) {
    GtkWidget *link = button("Read thread  ›", G_CALLBACK(open_replies), app, id);
    char *name = g_strdup_printf("Read thread, %d %s", replies, replies == 1 ? "reply" : "replies");
    action_name(link, name);
    g_free(name);
    GtkWidget *left = gtk_alignment_new(0, 0.5, 0, 0);
    gtk_container_add(GTK_CONTAINER(left), link);
    gtk_box_pack_start(GTK_BOX(list), left, FALSE, TRUE, 0);
}

#include "discussion.h"

static GtkWidget *scroll_body(App *app) {
    GtkWidget *scroll = gtk_scrolled_window_new(0, 0);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(app->body), scroll, TRUE, TRUE, 0);
    GtkWidget *viewport = gtk_viewport_new(0, 0);
    gtk_viewport_set_shadow_type(GTK_VIEWPORT(viewport), GTK_SHADOW_NONE);
    gtk_widget_set_name(viewport, "hnreader-paper");
    gtk_container_add(GTK_CONTAINER(scroll), viewport);
    app->content = scroll;
    GtkWidget *list = gtk_vbox_new(FALSE, 12);
    gtk_container_set_border_width(GTK_CONTAINER(list), LIST_INSET);
    gtk_container_add(GTK_CONTAINER(viewport), list);
    return list;
}

static void render_list(App *app, char *output, gboolean comments) {
    if (!output) output = g_strdup("");
    if (!comments && g_str_has_prefix(output, "page\t")) {
        char **page_header = g_strsplit(output, "\n", 2);
        char **fields = g_strsplit(page_header[0], "\t", 5);
        if (g_strv_length(fields) == 5) {
            char *stamp = decode(fields[3]);
            g_free(app->updated);
            app->updated = *stamp ? g_strdup_printf("Updated %s", stamp) : 0;
            g_free(stamp);
            if (!app->notice || !*app->notice)
                gtk_label_set_text(GTK_LABEL(app->status), app->updated ? app->updated : "");
            char *without_stamp = g_strdup_printf("page\t%s\t%s\t\t\n%s",
                fields[1], fields[2], page_header[1] ? page_header[1] : "");
            g_free(output);
            output = without_stamp;
        }
        g_strfreev(fields);
        g_strfreev(page_header);
    }
    if (app->last_output && g_str_equal(app->last_output, output)) {
        g_free(output);
        return;
    }
    if (comments) remember_comment_position(app);
    reset_output(app);
    app->last_output = output;
    if (comments) {
        mount_discussion(app, output);
        return;
    }
    if (app->content) gtk_widget_destroy(app->content);
    app->content = 0;
    GtkWidget *list = scroll_body(app);
    GtkWidget *scroll = app->content;
    app->content = list;
    int count = 0, page = 1, pages = 1;
    GtkWidget *rows_box = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(app->content), rows_box, FALSE, TRUE, 0);
    app->content = rows_box;
    app->first_id = 0;
    char **lines = g_strsplit(output, "\n", -1);
    for (int n = 0; lines[n] && *lines[n]; n++) {
        char **parts = g_strsplit(lines[n], "\t", -1);
        if (g_str_equal(parts[0], "page") && g_strv_length(parts) == 5) {
            char *page_text = decode(parts[1]);
            char *pages_text = decode(parts[2]);
            page = number(page_text);
            pages = number(pages_text);
            g_free(page_text);
            g_free(pages_text);
        } else if (g_str_equal(parts[0], "row") && g_strv_length(parts) == 6) {
            const char *fields[5];
            for (int i = 0; i < 5; i++) fields[i] = decode(parts[i + 1]);
            if (!app->first_id) app->first_id = number(fields[0]);
            GtkWidget *entry = feed_row(app, fields);
            gtk_widget_set_size_request(entry, -1, app->feed_row_height);
            gtk_box_pack_start(GTK_BOX(app->content), entry, FALSE, TRUE, 0);
            for (int i = 0; i < 5; i++) g_free((char *)fields[i]);
            count++;
        }
        g_strfreev(parts);
    }
    g_strfreev(lines);
    if (!count) {
        if (app->feed == 6) message(app, "Saved stories appear here. Open a story and tap Save.");
        else message(app, "No stories cached yet. Connect to Wi-Fi and tap Refresh.");
    }
    app->page = page;
    app->pages = pages;
    app->content = scroll;
    gtk_widget_show_all(scroll);
    pagination(app, page, pages, TRUE);
}

static void toggle_comment(GtkWidget *widget, App *app) {
    int id = value_of(widget);
    if (!app->last_output || id <= 0) return;
    gpointer key = GINT_TO_POINTER(id);
    if (!g_hash_table_remove(app->collapsed, key)) g_hash_table_insert(app->collapsed, key, key);
    char *snapshot = g_strdup(app->last_output);
    reset_output(app);
    render_list(app, snapshot, TRUE);
}

#include "discussion-io.h"

static void load_article(GtkWidget *widget, App *app) {
    (void)widget;
    remember_comment_position(app);
    select_screen(app, ARTICLE);
    char *id = g_strdup_printf("%d", app->story);
    char *offline = local_command(app, "cached-article", id, 0, 0, 0);
    g_free(id);
    if (offline) g_free(offline);
    else {
        say(app, "Downloading the publisher page…");
        request(app, (Task){LOAD_ARTICLE, app->revision, 0, app->story, 0, 0, 0, FALSE});
    }
}

#include "account.h"

static void bookmark(GtkWidget *widget, App *app) {
    (void)widget;
    char *id = g_strdup_printf("%d", app->story);
    if (app->username) {
        if (!app->account_watch)
            start_account_request(app, "favorite", id, 0);
    } else {
        char *result = local_command(app, "bookmark", id, 0, 0, 0);
        if (result) {
            app->discussion_saved = !app->discussion_saved;
            g_free(result);
        } else say(app, "Could not save the story.");
    }
    g_free(id);
    update_bookmark(app);
}

static void link_dialog(App *app, const char *address) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Publisher link", GTK_WINDOW(app->window),
        GTK_DIALOG_MODAL, (const char *)0, (void *)0);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog), action(ICON_ARROW_LEFT, "Close publisher link", 0, app, 0), GTK_RESPONSE_CLOSE);
    GtkWidget *link = label(address, 0);
    wrap(link);
    gtk_label_set_selectable(GTK_LABEL(link), TRUE);
    gtk_widget_set_size_request(link, 350, -1);
    gtk_container_set_border_width(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), 16);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), link, TRUE, TRUE, 0);
    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static void show_link(GtkWidget *widget, App *app) {
    (void)widget;
    if (app->article_url && *app->article_url) link_dialog(app, app->article_url);
}

static void browser_finished(GPid pid, gint status, gpointer data) {
    App *app = data;
    g_spawn_close_pid(pid);
    char *url = app->browser_url;
    app->browser_url = 0;
    if (status != 0 && url) {
        say(app, "Browser unavailable. Here is the link.");
        link_dialog(app, url);
    }
    g_free(url);
}

static void launch_browser(App *app, const char *url) {
    if (app->browser_url || !url || !*url) return;
    app->browser_url = g_strdup(url);
    char *argv[] = {(char *)app->backend, (char *)app->cache, "open-url", app->browser_url, 0};
    GPid pid = 0;
    gboolean started = g_spawn_async(0, argv, 0, G_SPAWN_DO_NOT_REAP_CHILD |
        G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL, 0, 0, &pid, 0);
    if (started) {
        g_child_watch_add(pid, browser_finished, app);
    } else {
        g_free(app->browser_url);
        app->browser_url = 0;
        say(app, "Couldn’t start the browser.");
    }
}

static void open_publisher(GtkWidget *widget, App *app) {
    (void)widget;
    launch_browser(app, app->article_url);
}

static void comment_link_clicked(GtkWidget *widget, App *app) {
    launch_browser(app, g_object_get_data(G_OBJECT(widget), "hn-url"));
}

static void render_article(App *app) {
    app->reading_position = 0;
    char *id = g_strdup_printf("%d", app->story);
    char *output = local_command(app, "cached-article", id, 0, 0, 0);
    g_free(id);
    if (app->content) gtk_widget_destroy(app->content);
    if (output && g_str_has_prefix(output, "article\t")) {
        GtkWidget *scroll = gtk_scrolled_window_new(0, 0);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_box_pack_start(GTK_BOX(app->body), scroll, TRUE, TRUE, 0);
        app->content = scroll;
        char *text = decode(g_strchomp(output + 8));
        GtkWidget *reader = gtk_text_view_new();
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(reader), GTK_WRAP_WORD_CHAR);
        gtk_text_view_set_editable(GTK_TEXT_VIEW(reader), FALSE);
        gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(reader), FALSE);
        reader_font(app, reader, reader_scale(app, ARTICLE), FALSE);
        gtk_text_view_set_left_margin(GTK_TEXT_VIEW(reader), 12);
        gtk_text_view_set_right_margin(GTK_TEXT_VIEW(reader), 12);
        gtk_text_view_set_pixels_below_lines(GTK_TEXT_VIEW(reader), 4);
        gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(reader)), text, -1);
        gtk_container_add(GTK_CONTAINER(scroll), reader);
        app->reading_position = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroll));
        g_signal_connect(app->reading_position, "changed", G_CALLBACK(update_scroll_controls), app);
        g_signal_connect(app->reading_position, "value-changed", G_CALLBACK(update_scroll_controls), app);
        g_free(text);
    } else {
        app->content = 0;
        clear_content(app, FALSE);
        message(app, "Readable publisher text appears here when available.");
        gtk_box_pack_start(GTK_BOX(app->content), action(ICON_LINK, "Show publisher link", G_CALLBACK(show_link), app, 0), FALSE, TRUE, 0);
    }
    g_free(output);
    gtk_widget_show_all(app->content);
    if (app->reading_position) show_scroll_controls(app, app->reading_position);
    else pagination(app, 1, 1, FALSE);
}

static void choose_feed(GtkWidget *widget, App *app) {
    int feed = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
    if (feed < 0 || feed == app->feed || app->screen != FEED) return;
    app->page = 1;
    app->first_id = 0;
    app->feed = feed;
    app->feed_size = fit_feed(app);
    select_screen(app, FEED);
    if (app->feed != 6) refresh_feed(app, FALSE);
}

static gboolean save_button_setting(App *app, const char *name, int value) {
    char *path = g_build_filename(app->cache, name, (void *)0);
    char *setting = g_strdup_printf("%d", value);
    gboolean saved = g_file_set_contents(path, setting, -1, 0);
    g_free(setting);
    g_free(path);
    say(app, saved ? "" : "Couldn’t save the page button setting.");
    return saved;
}

static void choose_page_buttons(GtkWidget *widget, App *app) {
    gboolean reversed = value_of(widget) != 0;
    if (!save_button_setting(app, "reverse-page-buttons", reversed)) return;
    app->reverse_page_buttons = reversed;
    render(app, 0);
}

static void choose_scroll_distance(GtkWidget *widget, App *app) {
    int divisor = value_of(widget);
    if (!save_button_setting(app, "scroll-distance", divisor)) return;
    app->scroll_divisor = divisor;
    render(app, 0);
}

static GtkWidget *setting_choice(const char *text, GCallback callback, App *app, int value, gboolean selected) {
    GtkWidget *choice = button(text, selected ? 0 : callback, app, value);
    g_object_set_data(G_OBJECT(choice), "hn-selected", GINT_TO_POINTER(selected));
    return choice;
}

static GtkWidget *new_picker(void) {
#if defined(GTK_MAJOR_VERSION) && GTK_MAJOR_VERSION >= 3
    return gtk_combo_box_text_new();
#else
    return gtk_combo_box_new_text();
#endif
}

static void picker_append(GtkWidget *picker, const char *text) {
#if defined(GTK_MAJOR_VERSION) && GTK_MAJOR_VERSION >= 3
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(picker), text);
#else
    gtk_combo_box_append_text(GTK_COMBO_BOX(picker), text);
#endif
}

#include "typography.h"

static void render_options(App *app) {
    if (app->content) gtk_widget_destroy(app->content);
    GtkWidget *list = scroll_body(app);
    GtkWidget *scroll = app->content;
    app->content = list;
    render_account(app);
    gtk_box_pack_start(GTK_BOX(list), gtk_hseparator_new(), FALSE, TRUE, 0);
    render_typography(app);
    gtk_box_pack_start(GTK_BOX(list), gtk_hseparator_new(), FALSE, TRUE, 0);
    message(app, "Scroll distance");
    GtkWidget *distances = gtk_hbox_new(TRUE, 8);
    for (int divisor = SCROLL_DISTANCE_COUNT; divisor >= 1; --divisor) {
        GtkWidget *choice = setting_choice(scroll_distances[divisor - 1], G_CALLBACK(choose_scroll_distance),
            app, divisor, divisor == app->scroll_divisor);
        gtk_box_pack_start(GTK_BOX(distances), choice, TRUE, TRUE, 0);
    }
    gtk_box_pack_start(GTK_BOX(app->content), distances, FALSE, TRUE, 0);
    message(app, "Physical page buttons");
    GtkWidget *page_buttons = gtk_hbox_new(TRUE, 8);
    GtkWidget *standard = setting_choice("Standard", G_CALLBACK(choose_page_buttons), app, 0, !app->reverse_page_buttons);
    GtkWidget *reversed = setting_choice("Reversed", G_CALLBACK(choose_page_buttons), app, 1, app->reverse_page_buttons);
    gtk_box_pack_start(GTK_BOX(page_buttons), standard, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(page_buttons), reversed, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(app->content), page_buttons, FALSE, TRUE, 0);
    app->content = scroll;
    gtk_widget_show_all(app->content);
    pagination(app, 1, 1, FALSE);
}

static void render(App *app, int anchor) {
    header(app);
    switch (app->screen) {
    case FEED:
        render_list(app, local_command(app, "view", feed_keys[app->feed], app->page, app->feed_size, anchor), FALSE);
        break;
    case COMMENTS:
        if (app->content) gtk_widget_destroy(app->content);
        app->content = 0;
        clear_content(app, FALSE);
        gtk_widget_show_all(app->content);
        pagination(app, 1, 1, FALSE);
        start_discussion_read(app);
        break;
    case ARTICLE:
        render_article(app);
        break;
    case OPTIONS:
        render_options(app);
        break;
    }
}

static void finished(GPid pid, gint status, gpointer data) {
    App *app = data;
    Task task = app->pending;
    g_spawn_close_pid(pid);
    app->active = FALSE;
    gboolean current = task.revision == app->revision;
    gboolean same_feed = task.operation == SYNC && app->screen == FEED && task.feed == app->feed;
    if (current || same_feed) {
        if (status != 0) {
            if (task.operation == LOAD_ARTICLE) say(app, "Article unavailable. You can still open the publisher link on another device.");
            else say(app, task.fresh ? "Couldn’t reach Hacker News. Showing cached stories." : "Offline · cached stories");
        } else {
            say(app, "");
        }
        if (status == 0 || task.operation == HYDRATE) render(app, current ? task.anchor : app->first_id);
    }
    if (app->queued && !app->active) {
        app->queued = FALSE;
        Task queued = *(Task *)g_object_get_data(G_OBJECT(app->window), "hn-next-task");
        if (queued.revision == app->revision) request(app, queued);
    }
}

static void request(App *app, Task task) {
    if (app->active) {
        Task *copy = g_malloc(sizeof(task));
        *copy = task;
        g_object_set_data_full(G_OBJECT(app->window), "hn-next-task", copy, g_free);
        app->queued = TRUE;
        return;
    }
    const char *operations[] = {"sync", "hydrate", "article"};
    char *id = g_strdup_printf("%d", task.id);
    char *page = g_strdup_printf("%d", task.page);
    char *size = g_strdup_printf("%d", task.size);
    char *anchor = g_strdup_printf("%d", task.anchor);
    const char *target = task.operation <= HYDRATE ? feed_keys[task.feed] : id;
    char *argv[] = {(char *)app->backend, (char *)app->cache, (char *)operations[task.operation],
                    (char *)target, page, size,
                    anchor, 0};
    GPid pid;
    gboolean started = g_spawn_async(0, argv, 0, G_SPAWN_DO_NOT_REAP_CHILD |
        G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL, 0, 0, &pid, 0);
    g_free(id);
    g_free(page);
    g_free(size);
    g_free(anchor);
    if (!started) {
        say(app, "Couldn’t start the data service.");
        return;
    }
    app->active = TRUE;
    app->pending = task;
    g_child_watch_add(pid, finished, app);
}

static void refresh_feed(App *app, gboolean manual) {
    if (app->feed == 6) {
        render(app, 0);
        return;
    }
    if (app->active && app->pending.operation == SYNC && app->pending.revision == app->revision) return;
    if (manual) say(app, "Refreshing…");
    int anchor = app->page > 1 ? app->first_id : 0;
    request(app, (Task){SYNC, app->revision, app->feed, 0, app->page, app->feed_size, anchor, manual});
}

static void refresh_clicked(GtkWidget *widget, App *app) {
    (void)widget;
    if (app->screen == FEED) refresh_feed(app, TRUE);

}

static void open_replies(GtkWidget *widget, App *app) {
    int id = value_of(widget);
    if (app->screen == COMMENTS) {
        if (app->depth >= 31) return;
        remember_comment_position(app);
        ++app->depth;
    } else {
        char *story_id = g_strdup_printf("%d", id);
        char *detail = local_command(app, "detail", story_id, 0, 0, 0);
        char **parts = detail ? g_strsplit(detail, "\t", 8) : 0;
        if (!parts || g_strv_length(parts) != 8) {
            say(app, "Story unavailable. Refresh this page to try again.");
            if (parts) g_strfreev(parts);
            g_free(detail);
            g_free(story_id);
            return;
        }
        g_free(app->discussion_text);
        g_free(app->discussion_title);
        app->discussion_title = decode(parts[2]);
        g_free(app->article_url);
        app->discussion_text = decode(parts[4]);
        app->article_url = decode(parts[5]);
        char *saved = decode(g_strchomp(parts[7]));
        app->discussion_saved = g_str_equal(saved, "true");
        app->discussion_favorite = FALSE;
        if (app->username) {
            char *favorite = local_command(app, "favorite-status", story_id, 0, 0, 0);
            app->discussion_favorite = favorite && g_str_has_prefix(favorite, "favorite\t");
            g_free(favorite);
        }
        g_free(saved);
        g_strfreev(parts);
        g_free(detail);
        g_free(story_id);
        app->story = id;
        app->depth = 0;
    }
    app->comment_id[app->depth] = id;
    app->comment_anchor[app->depth] = 0;
    app->comment_offset[app->depth] = 0;
    select_screen(app, COMMENTS);
}

static void navigate(App *app, int delta) {
    if (app->screen == FEED) {
        int page = app->page + delta;
        if (page < 1 || page > app->pages) return;
        app->page = page;
        app->revision++;
        say(app, "");
        render(app, 0);
        hydrate_feed_page(app);
        return;
    }
    GtkAdjustment *scroll = app->screen == COMMENTS ? app->comment_scroll :
        app->screen == ARTICLE ? app->reading_position : 0;
    if (!scroll) return;
    double step = gtk_adjustment_get_page_size(scroll) / app->scroll_divisor;
    gtk_adjustment_set_value(scroll, gtk_adjustment_get_value(scroll) + delta * step);
}

static void turn_page(GtkWidget *widget, App *app) {
    navigate(app, value_of(widget));
}

#ifdef __linux__
#include "page-buttons.h"
#endif

static void go_back(GtkWidget *widget, App *app) {
    (void)widget;
    if (app->screen == FEED) {
        app->exit_status = RETURN_TO_KINDLE_HOME;
        gtk_widget_destroy(app->window);
    } else if (app->screen == COMMENTS && app->depth > 0) {
        app->depth--;
        select_screen(app, COMMENTS);
    } else if (app->screen == ARTICLE) {
        select_screen(app, COMMENTS);
    } else if (app->screen == OPTIONS) {
        app->feed_size = fit_feed(app);
        select_screen(app, FEED);
        hydrate_feed_page(app);
    } else {
        select_screen(app, FEED);
    }
}

static void show_options(GtkWidget *widget, App *app) {
    (void)widget;
    if (app->screen == FEED) select_screen(app, OPTIONS);
    else select_screen(app, FEED);
}

static gboolean start(gpointer data) {
    App *app = data;
    app->ready = TRUE;
    app->feed_size = fit_feed(app);
    render(app, 0);
    refresh_feed(app, FALSE);
    return FALSE;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        g_printerr("Usage: hnreader PATH-TO-DATA-BINARY CACHE-DIR\n");
        return 2;
    }
    App app = {0};
    app.backend = argv[1];
    app.cache = argv[2];
    app.scroll_divisor = 2;
    app.page = 1;
    if (!gtk_init_check(&argc, &argv)) {
        g_printerr("HNink could not connect to the display.\n");
        return 1;
    }
    g_mkdir_with_parents(app.cache, 0755);
    app.collapsed = g_hash_table_new(0, 0);
    load_typography(&app);
    load_account(&app);
    char *path = g_build_filename(app.cache, "reverse-page-buttons", (void *)0);
    char *setting = 0;
    if (g_file_get_contents(path, &setting, 0, 0))
        app.reverse_page_buttons = number(setting) == 1;
    g_free(setting);
    g_free(path);

    path = g_build_filename(app.cache, "scroll-distance", (void *)0);
    setting = 0;
    if (g_file_get_contents(path, &setting, 0, 0)) {
        int divisor = number(setting);
        if (divisor >= 1 && divisor <= SCROLL_DISTANCE_COUNT) app.scroll_divisor = divisor;
    }
    g_free(setting);
    g_free(path);

    gtk_rc_parse_string(
        "style \"hnreader-paper\" { bg[NORMAL] = \"#ffffff\" } "
        "widget \"*hnreader-paper\" style \"hnreader-paper\" "
        "style \"hnreader-scrollbar\" { "
        "GtkRange::slider-width = 14 GtkRange::trough-border = 2 GtkScrollbar::min-slider-length = 64 "
        "bg[NORMAL] = \"#303030\" bg[ACTIVE] = \"#dedede\" bg[PRELIGHT] = \"#202020\" } "
        "widget \"*hnreader-discussion-scrollbar\" style \"hnreader-scrollbar\"");
#if defined(GTK_MAJOR_VERSION) && GTK_MAJOR_VERSION >= 3
    GtkCssProvider *paper = gtk_css_provider_new();
    gtk_css_provider_load_from_data(paper,
        "#hnreader-paper { background: #fff; color: #191919; }"
        "#hnreader-discussion-scrollbar trough { background: #e0e0e0; min-width: 12px; }"
        "#hnreader-discussion-scrollbar slider { background: #333; min-width: 12px; min-height: 56px; }",
        -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(paper),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(paper);
#endif
    app.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(app.window, "hnreader-paper");
    gtk_widget_ensure_style(app.window);
    app.ui_font = pango_font_description_copy(
        pango_context_get_font_description(gtk_widget_get_pango_context(app.window)));
    find_font_families(&app);
    int line_height = 0;
    measure_text(&app, app.ui_font, "HgjÅ", 0, &line_height);
    app.icon_size = line_height > 28 ? line_height : 28;
    app.tap_size = app.icon_size + app.icon_size / 2;
    if (app.tap_size < 52) app.tap_size = 52;
#ifdef __linux__
    int width = gdk_screen_get_width(gdk_screen_get_default());
    int height = gdk_screen_get_height(gdk_screen_get_default());
    int short_edge = width < height ? width : height;
    if (app.icon_size < short_edge / 24) app.icon_size = short_edge / 24;
    if (app.tap_size < short_edge / 15) app.tap_size = short_edge / 15;
#endif
#ifdef __linux__
    gtk_window_maximize(GTK_WINDOW(app.window));
#else
    gtk_window_set_title(GTK_WINDOW(app.window), "HNink");
    gtk_window_set_default_size(GTK_WINDOW(app.window), 650, 850);
#endif
    g_signal_connect(app.window, "destroy", G_CALLBACK(gtk_main_quit), 0);

    GtkWidget *stack = gtk_vbox_new(FALSE, 10);
    gtk_container_set_border_width(GTK_CONTAINER(stack), 16);
    gtk_container_add(GTK_CONTAINER(app.window), stack);
    app.header = gtk_hbox_new(FALSE, 12);
    app.left = action(ICON_ARROW_LEFT, "Back to Kindle Home", G_CALLBACK(go_back), &app, 0);
    app.heading = label("HNink", "<span size='x-large' weight='bold'>%s</span>");
    gtk_label_set_ellipsize(GTK_LABEL(app.heading), PANGO_ELLIPSIZE_END);
    app.refresh = action(ICON_REFRESH, "Refresh", G_CALLBACK(refresh_clicked), &app, 0);
    gtk_box_pack_start(GTK_BOX(app.header), app.left, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(app.header), app.heading, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(app.header), app.refresh, FALSE, FALSE, 0);
    app.scroll_up = action(ICON_UP, "Scroll up", G_CALLBACK(turn_page), &app, -1);
    gtk_box_pack_end(GTK_BOX(app.header), app.scroll_up, FALSE, FALSE, 0);
    app.discussion_save = action(ICON_SAVE, "Save story", G_CALLBACK(bookmark), &app, 0);
    gtk_box_pack_end(GTK_BOX(app.header), app.discussion_save, FALSE, FALSE, 0);
    app.discussion_globe = action(ICON_GLOBE, "Open article in browser", G_CALLBACK(open_publisher), &app, 0);
    gtk_box_pack_end(GTK_BOX(app.header), app.discussion_globe, FALSE, FALSE, 0);
    app.discussion_book = action(ICON_BOOK, "Read in HNink", G_CALLBACK(load_article), &app, 0);
    gtk_box_pack_end(GTK_BOX(app.header), app.discussion_book, FALSE, FALSE, 0);
    app.options = action(ICON_GEAR, "Settings", G_CALLBACK(show_options), &app, 0);
    gtk_box_pack_end(GTK_BOX(app.header), app.options, FALSE, FALSE, 0);
    GtkWidget *header_block = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header_block), app.header, FALSE, TRUE, 0);
    app.heading_divider = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(header_block), app.heading_divider, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(stack), header_block, FALSE, TRUE, 0);

    app.status_rail = gtk_hbox_new(FALSE, app.icon_size / 2);
    app.feed_picker = new_picker();
    for (unsigned int i = 0; i < sizeof(feed_names) / sizeof(feed_names[0]); ++i)
        picker_append(app.feed_picker, feed_names[i]);
    gtk_combo_box_set_active(GTK_COMBO_BOX(app.feed_picker), app.feed);
    action_name(app.feed_picker, "Feed");
    g_signal_connect(app.feed_picker, "changed", G_CALLBACK(choose_feed), &app);
    gtk_widget_set_size_request(app.feed_picker, -1, app.tap_size);
    app.status = label("", 0);
    gtk_label_set_ellipsize(GTK_LABEL(app.status), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(app.status_rail), app.feed_picker, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(app.status_rail), app.status, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(stack), app.status_rail, FALSE, TRUE, 0);
    app.rail_divider = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(stack), app.rail_divider, FALSE, FALSE, 0);

    app.body = gtk_vbox_new(FALSE, 0);
    g_signal_connect(app.body, "size-allocate", G_CALLBACK(feed_body_allocated), &app);
    gtk_box_pack_start(GTK_BOX(stack), app.body, TRUE, TRUE, 0);
    app.footer = gtk_hbox_new(FALSE, 10);
    app.previous = action(ICON_BACK, "Previous page", G_CALLBACK(turn_page), &app, -1);
    app.next = action(ICON_NEXT, "Next page", G_CALLBACK(turn_page), &app, 1);
    app.page_label = gtk_label_new("");
    GtkAccelGroup *keys = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(app.window), keys);
    gtk_widget_add_accelerator(app.left, "clicked", keys, 0xff1b, 0, 0);
#ifdef __linux__
    app.page_buttons_watch = watch_page_buttons(&app);
    gtk_window_set_title(GTK_WINDOW(app.window), app.page_buttons_watch
        ? "L:A_N:application_ID:org.hnink.kindle_PC:T_O:UD"
        : "L:A_N:application_ID:org.hnink.kindle_PC:T");
#endif
    if (!app.page_buttons_watch) {
        gtk_widget_add_accelerator(app.previous, "clicked", keys, 0xff55, 0, 0);
        gtk_widget_add_accelerator(app.scroll_up, "clicked", keys, 0xff55, 0, 0);
        gtk_widget_add_accelerator(app.next, "clicked", keys, 0xff56, 0, 0);
    }
    gtk_widget_add_accelerator(app.refresh, "clicked", keys, 0xffc2, 0, 0);
    g_object_unref(keys);
    gtk_box_pack_start(GTK_BOX(app.footer), app.previous, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(app.footer), app.page_label, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(app.footer), app.next, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(stack), app.footer, FALSE, TRUE, 0);
    gtk_widget_show_all(app.window);
    header(&app);
    g_idle_add(start, &app);
    gtk_main();
    if (app.account_watch) g_source_remove(app.account_watch);
    g_free(app.username);
    if (app.page_buttons_watch) g_source_remove(app.page_buttons_watch);
#ifdef __linux__
    if (app.button_orientation_watch) g_source_remove(app.button_orientation_watch);
    if (app.button_steps) g_string_free(app.button_steps, TRUE);
#endif
    if (app.feed_layout_idle) g_source_remove(app.feed_layout_idle);
    pango_font_description_free(app.ui_font);
    stop_discussion_read(&app);
    clear_comment_window(&app);
    g_hash_table_destroy(app.collapsed);
    g_free(app.browser_url);
    g_free(app.last_output);
    g_free(app.notice);
    g_free(app.article_url);
    g_free(app.discussion_text);
    g_free(app.discussion_title);
    g_free(app.updated);
    return app.exit_status;
}
