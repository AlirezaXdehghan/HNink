typedef enum { ICON_NONE, ICON_ARROW_LEFT, ICON_BACK, ICON_NEXT, ICON_REFRESH, ICON_GEAR,
    ICON_CHAT, ICON_BOOK, ICON_LINK, ICON_SAVE, ICON_SAVED, ICON_GLOBE, ICON_UP, ICON_DOWN, ICON_MINUS, ICON_PLUS,
    ICON_IMAGE } Icon;

/* A shipped monochrome PNG, painted through its alpha in the icon ink. */
static void paint_image(GtkWidget *area, cairo_t *cr, int size, double ink) {
    cairo_surface_t *image = g_object_get_data(G_OBJECT(area), "hn-image");
    int width = image ? cairo_image_surface_get_width(image) : 0;
    if (width <= 0) return;
    cairo_scale(cr, (double)size / width, (double)size / width);
    cairo_set_source_rgb(cr, ink, ink, ink);
    cairo_mask_surface(cr, image, 0, 0);
}

static void icon_path(cairo_t *cr, double a, double b, double c, double d, double e, double f) {
    cairo_move_to(cr, a, b);
    cairo_line_to(cr, c, d);
    cairo_line_to(cr, e, f);
}

static void paint_icon(GtkWidget *area, cairo_t *cr) {
    GtkAllocation bounds;
    gtk_widget_get_allocation(area, &bounds);
    cairo_save(cr);
    int size = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(area), "hn-icon-size"));
    cairo_translate(cr, (bounds.width - size) / 2.0, (bounds.height - size) / 2.0);
    double ink = gtk_widget_is_sensitive(area) ? 0.09 : 0.65;
    if (g_object_get_data(G_OBJECT(area), "hn-icon") == GINT_TO_POINTER(ICON_IMAGE)) {
        paint_image(area, cr, size, ink);
        cairo_restore(cr);
        return;
    }
    double scale = size / 24.0;
    cairo_scale(cr, scale, scale);
    cairo_set_source_rgb(cr, ink, ink, ink);
    cairo_set_line_width(cr, 1.75);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    switch (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(area), "hn-icon"))) {
    case ICON_NONE:
    case ICON_IMAGE: break;
    case ICON_ARROW_LEFT:
        icon_path(cr, 11.5, 5, 4.5, 12, 11.5, 19);
        cairo_move_to(cr, 4.5, 12);
        cairo_line_to(cr, 20, 12);
        break;
    case ICON_UP: icon_path(cr, 5, 15, 12, 8, 19, 15); break;
    case ICON_DOWN: icon_path(cr, 5, 9, 12, 16, 19, 9); break;
    case ICON_PLUS:
        cairo_move_to(cr, 12, 5); cairo_line_to(cr, 12, 19);
        /* fall through */
    case ICON_MINUS:
        cairo_move_to(cr, 5, 12); cairo_line_to(cr, 19, 12);
        break;
    case ICON_BACK: icon_path(cr, 15, 5, 8, 12, 15, 19); break;
    case ICON_NEXT: icon_path(cr, 9, 5, 16, 12, 9, 19); break;
    case ICON_REFRESH:
        cairo_arc(cr, 12, 12, 7.5, 0.05, 5.52);
        cairo_new_sub_path(cr);
        icon_path(cr, 17.5, 3, 18, 8, 13, 8);
        break;
    case ICON_GEAR: {
        static const double outline[][2] = {
            {10, 4.6}, {10.5, 2.5}, {13.5, 2.5}, {14, 4.6},
            {15.8, 5.4}, {17.7, 4.2}, {19.8, 6.3}, {18.6, 8.2},
            {19.4, 10}, {21.5, 10.5}, {21.5, 13.5}, {19.4, 14},
            {18.6, 15.8}, {19.8, 17.7}, {17.7, 19.8}, {15.8, 18.6},
            {14, 19.4}, {13.5, 21.5}, {10.5, 21.5}, {10, 19.4},
            {8.2, 18.6}, {6.3, 19.8}, {4.2, 17.7}, {5.4, 15.8},
            {4.6, 14}, {2.5, 13.5}, {2.5, 10.5}, {4.6, 10},
            {5.4, 8.2}, {4.2, 6.3}, {6.3, 4.2}, {8.2, 5.4},
        };
        cairo_move_to(cr, outline[0][0], outline[0][1]);
        for (unsigned int i = 1; i < sizeof(outline) / sizeof(outline[0]); ++i)
            cairo_line_to(cr, outline[i][0], outline[i][1]);
        cairo_close_path(cr);
        cairo_new_sub_path(cr);
        cairo_arc(cr, 12, 12, 3.2, 0, 6.284);
        break;
    }
    case ICON_CHAT:
        cairo_move_to(cr, 4, 5); cairo_line_to(cr, 20, 5);
        cairo_line_to(cr, 20, 16); cairo_line_to(cr, 11, 16);
        cairo_line_to(cr, 6, 20); cairo_line_to(cr, 6, 16);
        cairo_line_to(cr, 4, 16); cairo_close_path(cr);
        break;
    case ICON_GLOBE:
        cairo_arc(cr, 12, 12, 8.5, 0, 6.284);
        cairo_move_to(cr, 4.5, 12);
        cairo_line_to(cr, 19.5, 12);
        cairo_move_to(cr, 12, 3.5);
        cairo_curve_to(cr, 7.3, 7.5, 7.3, 16.5, 12, 20.5);
        cairo_curve_to(cr, 16.7, 16.5, 16.7, 7.5, 12, 3.5);
        break;
    case ICON_BOOK:
        icon_path(cr, 12, 6, 4, 4, 4, 17);
        icon_path(cr, 4, 17, 12, 20, 20, 17);
        icon_path(cr, 20, 17, 20, 4, 12, 6);
        cairo_line_to(cr, 12, 20);
        break;
    case ICON_LINK:
        icon_path(cr, 13, 5, 19, 5, 19, 11);
        cairo_move_to(cr, 10, 14); cairo_line_to(cr, 19, 5);
        icon_path(cr, 9, 6, 5, 6, 5, 19);
        cairo_line_to(cr, 18, 19); cairo_line_to(cr, 18, 15);
        break;
    case ICON_SAVE:
    case ICON_SAVED:
        cairo_move_to(cr, 6, 4); cairo_line_to(cr, 18, 4);
        cairo_line_to(cr, 18, 20); cairo_line_to(cr, 12, 16);
        cairo_line_to(cr, 6, 20); cairo_close_path(cr);
        if (g_object_get_data(G_OBJECT(area), "hn-icon") == GINT_TO_POINTER(ICON_SAVED))
            cairo_fill_preserve(cr);
        break;
    }
    cairo_stroke(cr);
    cairo_restore(cr);
}

#if defined(GTK_MAJOR_VERSION) && GTK_MAJOR_VERSION >= 3
static gboolean draw_icon(GtkWidget *area, cairo_t *cr, gpointer data) {
    (void)data;
    paint_icon(area, cr);
    return FALSE;
}
#else
static gboolean draw_icon(GtkWidget *area, void *event, gpointer data) {
    (void)event;
    (void)data;
    cairo_t *cr = gdk_cairo_create(gtk_widget_get_window(area));
    GtkAllocation bounds;
    gtk_widget_get_allocation(area, &bounds);
    cairo_translate(cr, bounds.x, bounds.y);
    cairo_rectangle(cr, 0, 0, bounds.width, bounds.height);
    cairo_clip(cr);
    paint_icon(area, cr);
    cairo_destroy(cr);
    return FALSE;
}
#endif

static GtkWidget *icon_widget(Icon icon, int size) {
    GtkWidget *area = gtk_drawing_area_new();
    gtk_widget_set_has_window(area, FALSE);
    gtk_widget_set_size_request(area, size, size);
    g_object_set_data(G_OBJECT(area), "hn-icon-size", GINT_TO_POINTER(size));
    g_object_set_data(G_OBJECT(area), "hn-icon", GINT_TO_POINTER(icon));
#if defined(GTK_MAJOR_VERSION) && GTK_MAJOR_VERSION >= 3
    g_signal_connect(area, "draw", G_CALLBACK(draw_icon), 0);
#else
    g_signal_connect(area, "expose-event", G_CALLBACK(draw_icon), 0);
#endif
    return area;
}

static void action_name(GtkWidget *widget, const char *name) {
    gtk_widget_set_tooltip_text(widget, name);
    atk_object_set_name(gtk_widget_get_accessible(widget), name);
}
