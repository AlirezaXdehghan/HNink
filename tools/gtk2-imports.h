/*
 * GTK+ 2 / GLib public ARM EABI imports used by the sysroot-free linker.
 * The normal Meson build uses the official GTK+ headers instead.
 * All GTK objects are opaque. No library struct layouts are assumed.
 */
typedef void GtkWidget;
typedef void GtkAdjustment;
typedef void GtkAccelGroup;
typedef void *gpointer;
typedef unsigned char guchar;
typedef unsigned int guint;
typedef unsigned int gsize;
typedef int gint;
typedef int gboolean;
typedef int GPid;
typedef void (*GCallback)(void);
typedef void (*GDestroyNotify)(gpointer);
typedef gboolean (*GSourceFunc)(gpointer);
typedef void (*GChildWatchFunc)(GPid, gint, gpointer);

#define TRUE 1
#define FALSE 0
#define GTK_WINDOW_TOPLEVEL 0
#define GTK_DIALOG_MODAL 1
#define GTK_RESPONSE_CLOSE (-7)
#define GTK_RELIEF_NONE 2
#define GTK_POLICY_ALWAYS 0
#define GTK_POLICY_AUTOMATIC 1
#define GTK_POLICY_NEVER 2
#define GTK_SHADOW_NONE 0
#define GTK_WRAP_WORD_CHAR 3
#define PANGO_WRAP_WORD_CHAR 2
#define PANGO_ELLIPSIZE_END 3
#define G_SPAWN_DO_NOT_REAP_CHILD (1 << 1)
#define G_SPAWN_SEARCH_PATH (1 << 2)
#define G_SPAWN_STDOUT_TO_DEV_NULL (1 << 3)
#define G_SPAWN_STDERR_TO_DEV_NULL (1 << 4)
#define GTK_BOX(x) (x)
#define GTK_BIN(x) (x)
#define GTK_LABEL(x) (x)
#define GTK_MISC(x) (x)
#define GTK_BUTTON(x) (x)
#define GTK_ENTRY(x) (x)
#define GTK_EDITABLE(x) (x)
#define GTK_COMBO_BOX(x) (x)
#define GTK_CONTAINER(x) (x)
#define GTK_SCROLLED_WINDOW(x) (x)
#define GTK_VIEWPORT(x) (x)
#define GTK_WINDOW(x) (x)
#define GTK_DIALOG(x) (x)
#define GTK_TEXT_VIEW(x) (x)
#define GTK_TABLE(x) (x)
#define G_OBJECT(x) (x)
#define GINT_TO_POINTER(x) ((gpointer)(x))
#define GPOINTER_TO_INT(x) ((int)(x))
#define G_CALLBACK(x) ((GCallback)(x))
#define g_signal_connect(o,s,f,d) g_signal_connect_data(o,s,f,d,0,0)

void *gdk_screen_get_default(void);
int gdk_screen_get_height(void *);
guchar *g_base64_decode(const char *, gsize *);
char *g_strndup(const char *, gsize);
char *g_strdup(const char *);
char *g_strdup_printf(const char *, ...);
gboolean g_utf8_validate(const char *, long, const char **);
long long g_ascii_strtoll(const char *, char **, guint);
char *g_markup_escape_text(const char *, long);
gboolean g_spawn_sync(const char *, char **, char **, int, void (*)(gpointer), gpointer, char **, char **, gint *, void *);
gboolean g_spawn_async(const char *, char **, char **, int, void (*)(gpointer), gpointer, GPid *, void *);
void g_spawn_close_pid(GPid);
guint g_child_watch_add(GPid, GChildWatchFunc, gpointer);
guint g_idle_add(GSourceFunc, gpointer);
void *g_malloc(gsize);
void g_free(gpointer);
gboolean g_str_equal(const void *, const void *);
gboolean g_str_has_prefix(const char *, const char *);
char *g_strchomp(char *);
char **g_strsplit(const char *, const char *, gint);
guint g_strv_length(char **);
void g_strfreev(char **);
char *g_build_filename(const char *, ...);
gint g_mkdir_with_parents(const char *, gint);
gboolean g_file_set_contents(const char *, const char *, long, void *);
gboolean g_file_get_contents(const char *, char **, gsize *, void *);
void g_printerr(const char *, ...);
void g_object_set_data(void *, const char *, gpointer);
void g_object_set_data_full(void *, const char *, gpointer, GDestroyNotify);
gpointer g_object_get_data(void *, const char *);
unsigned long g_signal_connect_data(void *, const char *, GCallback, gpointer, void *, gint);

gboolean gtk_init_check(gint *, char ***);
void gtk_main(void);
void gtk_main_quit(void);
void gtk_rc_parse_string(const char *);
GtkWidget *gtk_label_new(const char *);
void gtk_misc_set_alignment(void *, float, float);
void gtk_label_set_markup(void *, const char *);
void gtk_label_set_text(void *, const char *);
void gtk_label_set_line_wrap(void *, gboolean);
void gtk_label_set_line_wrap_mode(void *, int);
void gtk_label_set_ellipsize(void *, int);
void gtk_label_set_selectable(void *, gboolean);
GtkWidget *gtk_button_new_with_label(const char *);
GtkWidget *gtk_combo_box_new_text(void);
void gtk_combo_box_append_text(void *, const char *);
void gtk_combo_box_set_active(void *, int);
int gtk_combo_box_get_active(void *);
GtkWidget *gtk_entry_new(void);
const char *gtk_entry_get_text(void *);
void gtk_entry_set_text(void *, const char *);
void gtk_entry_set_width_chars(void *, int);
void gtk_entry_set_max_length(void *, int);
void gtk_entry_set_visibility(void *, gboolean);
void gtk_button_set_focus_on_click(void *, gboolean);
int gtk_editable_get_position(void *);
gboolean gtk_editable_get_selection_bounds(void *, int *, int *);
void gtk_editable_delete_text(void *, int, int);
void gtk_editable_insert_text(void *, const char *, int, int *);
void gtk_editable_set_position(void *, int);
void gtk_widget_set_no_show_all(void *, gboolean);
void gtk_button_set_relief(void *, int);
void gtk_button_set_label(void *, const char *);
GtkWidget *gtk_bin_get_child(void *);
void gtk_widget_set_size_request(GtkWidget *, gint, gint);
void gtk_widget_set_sensitive(GtkWidget *, gboolean);
void gtk_widget_destroy(GtkWidget *);
void gtk_widget_show(GtkWidget *);
void gtk_widget_show_all(GtkWidget *);
void gtk_widget_hide(GtkWidget *);
GtkWidget *gtk_vbox_new(gboolean, gint);
GtkWidget *gtk_hbox_new(gboolean, gint);
GtkWidget *gtk_hseparator_new(void);
void gtk_box_pack_start(void *, GtkWidget *, gboolean, gboolean, guint);
void gtk_box_pack_end(void *, GtkWidget *, gboolean, gboolean, guint);
void gtk_container_add(void *, GtkWidget *);
void gtk_container_propagate_expose(void *, GtkWidget *, void *);
void gtk_container_set_border_width(void *, guint);
GtkWidget *gtk_scrolled_window_new(void *, void *);
void gtk_scrolled_window_set_policy(void *, int, int);
GtkWidget *gtk_viewport_new(void *, void *);
void gtk_viewport_set_shadow_type(void *, int);
GtkWidget *gtk_dialog_new_with_buttons(const char *, void *, int, const char *, ...);
GtkWidget *gtk_dialog_get_content_area(void *);
gint gtk_dialog_run(void *);
void gtk_dialog_add_action_widget(void *, GtkWidget *, gint);
GtkWidget *gtk_text_view_new(void);
void gtk_text_view_set_wrap_mode(void *, int);
void gtk_text_view_set_editable(void *, gboolean);
void gtk_text_view_set_cursor_visible(void *, gboolean);
void gtk_text_view_set_left_margin(void *, gint);
void gtk_text_view_set_right_margin(void *, gint);
void gtk_text_view_set_pixels_below_lines(void *, gint);
void *gtk_text_view_get_buffer(void *);
void gtk_text_buffer_set_text(void *, const char *, gint);
GtkWidget *gtk_table_new(guint, guint, gboolean);
void gtk_table_set_row_spacings(void *, guint);
void gtk_table_set_col_spacings(void *, guint);
void gtk_table_attach_defaults(void *, GtkWidget *, guint, guint, guint, guint);
GtkWidget *gtk_window_new(int);
void gtk_window_set_title(void *, const char *);
void gtk_window_maximize(void *);
void gtk_window_set_default_size(void *, gint, gint);

GtkAdjustment *gtk_scrolled_window_get_vadjustment(void *);
double gtk_adjustment_get_page_size(GtkAdjustment *);
double gtk_adjustment_get_upper(GtkAdjustment *);
double gtk_adjustment_get_value(GtkAdjustment *);
void gtk_adjustment_set_value(GtkAdjustment *, double);
GtkAccelGroup *gtk_accel_group_new(void);
void gtk_window_add_accel_group(void *, GtkAccelGroup *);
void gtk_widget_add_accelerator(GtkWidget *, const char *, GtkAccelGroup *, guint, int, int);
void g_object_unref(void *);

typedef void cairo_t;
typedef struct { int x, y, width, height; } GtkAllocation;
#define CAIRO_LINE_CAP_ROUND 1
#define CAIRO_LINE_JOIN_ROUND 1
GtkWidget *gtk_drawing_area_new(void);
GtkWidget *gtk_vseparator_new(void);
GtkWidget *gtk_alignment_new(float, float, float, float);
void gtk_widget_set_name(GtkWidget *, const char *);
void gtk_widget_queue_draw(GtkWidget *);
void gtk_widget_get_allocation(GtkWidget *, GtkAllocation *);
void *gtk_widget_get_window(GtkWidget *);
void *gtk_widget_get_accessible(GtkWidget *);
gboolean gtk_widget_is_sensitive(GtkWidget *);
void gtk_widget_set_tooltip_text(GtkWidget *, const char *);
void atk_object_set_name(void *, const char *);
cairo_t *gdk_cairo_create(void *);
void cairo_save(cairo_t *);
void cairo_restore(cairo_t *);
void cairo_translate(cairo_t *, double, double);
void cairo_set_source_rgb(cairo_t *, double, double, double);
void cairo_set_line_width(cairo_t *, double);
void cairo_set_line_cap(cairo_t *, int);
void cairo_set_line_join(cairo_t *, int);
void cairo_move_to(cairo_t *, double, double);
void cairo_line_to(cairo_t *, double, double);
void cairo_arc(cairo_t *, double, double, double, double, double);
void cairo_new_sub_path(cairo_t *);
void cairo_close_path(cairo_t *);
void cairo_fill_preserve(cairo_t *);
void cairo_stroke(cairo_t *);
void cairo_destroy(cairo_t *);

void cairo_curve_to(cairo_t *, double, double, double, double, double, double);

void gtk_widget_set_has_window(GtkWidget *, gboolean);
void cairo_rectangle(cairo_t *, double, double, double, double);
void cairo_clip(cairo_t *);

typedef void PangoContext;
typedef void PangoFontDescription;
typedef void PangoFontFamily;
typedef struct { int width, height; } GtkRequisition;
PangoContext *gtk_widget_get_pango_context(GtkWidget *);
void gtk_widget_modify_font(GtkWidget *, const PangoFontDescription *);
void gtk_widget_size_request(GtkWidget *, GtkRequisition *);
const PangoFontDescription *pango_context_get_font_description(PangoContext *);
void pango_context_list_families(PangoContext *, PangoFontFamily ***, int *);
const char *pango_font_family_get_name(PangoFontFamily *);
void pango_font_description_set_family(PangoFontDescription *, const char *);
void pango_font_description_set_weight(PangoFontDescription *, int);
int g_ascii_strcasecmp(const char *, const char *);
PangoFontDescription *pango_font_description_copy(const PangoFontDescription *);
int pango_font_description_get_size(const PangoFontDescription *);
gboolean pango_font_description_get_size_is_absolute(const PangoFontDescription *);
void pango_font_description_set_size(PangoFontDescription *, int);
void pango_font_description_set_absolute_size(PangoFontDescription *, double);
void pango_font_description_free(PangoFontDescription *);
gboolean g_source_remove(guint);

typedef void PangoLayout;
void gtk_widget_ensure_style(GtkWidget *);
int gdk_screen_get_width(void *);
PangoLayout *gtk_widget_create_pango_layout(GtkWidget *, const char *);
void pango_layout_set_font_description(PangoLayout *, const PangoFontDescription *);
void pango_layout_set_markup(PangoLayout *, const char *, int);
void pango_layout_get_pixel_size(PangoLayout *, int *, int *);
void cairo_scale(cairo_t *, double, double);

#define GTK_LAYOUT(x) (x)
#define PANGO_SCALE 1024
GtkWidget *gtk_layout_new(GtkAdjustment *, GtkAdjustment *);
void gtk_layout_put(void *, GtkWidget *, int, int);
void gtk_layout_move(void *, GtkWidget *, int, int);
void gtk_layout_set_size(void *, guint, guint);
PangoLayout *gtk_label_get_layout(void *);
PangoLayout *pango_layout_copy(PangoLayout *);
void pango_layout_set_width(PangoLayout *, int);
void pango_layout_set_wrap(PangoLayout *, int);

#define GPOINTER_TO_UINT(x) ((guint)(x))
#define GUINT_TO_POINTER(x) ((gpointer)(x))
#define CAIRO_OPERATOR_DIFFERENCE 23
typedef struct {
    int type;
    void *window;
    signed char send_event;
    GtkAllocation area;
    void *region;
    int count;
} GdkEventExpose;
guint g_timeout_add(guint, GSourceFunc, gpointer);
void cairo_set_operator(cairo_t *, int);
void gdk_cairo_region(cairo_t *, const void *);
void gdk_window_process_updates(void *, gboolean);
void cairo_fill(cairo_t *);
void *g_malloc0(gsize);

typedef void GHashTable;
typedef void GIOChannel;
typedef void GDir;
typedef int GIOStatus;
typedef int GIOCondition;
typedef struct { char *str; gsize len, allocated_len; } GString;
#define G_IO_STATUS_NORMAL 1
#define G_IO_STATUS_AGAIN 3
#define G_IO_IN 1
#define G_IO_ERR 8
#define G_IO_HUP 16
#define G_IO_FLAG_NONBLOCK 2
#define G_PRIORITY_DEFAULT_IDLE 200
gboolean g_spawn_async_with_pipes(const char *, char **, char **, int, void *,
    gpointer, GPid *, gint *, gint *, gint *, void *);
GIOChannel *g_io_channel_unix_new(int);
void g_io_channel_set_close_on_unref(GIOChannel *, gboolean);
GIOStatus g_io_channel_set_encoding(GIOChannel *, const char *, void *);
void g_io_channel_set_buffered(GIOChannel *, gboolean);
GIOStatus g_io_channel_set_flags(GIOChannel *, int, void *);
GIOStatus g_io_channel_read_chars(GIOChannel *, char *, gsize, gsize *, void *);
GIOStatus g_io_channel_read_line(GIOChannel *, char **, gsize *, gsize *, void *);
GIOStatus g_io_channel_write_chars(GIOChannel *, const char *, long, gsize *, void *);
GIOStatus g_io_channel_shutdown(GIOChannel *, gboolean, void *);
guint g_io_add_watch_full(GIOChannel *, int, GIOCondition,
    gboolean (*)(GIOChannel *, GIOCondition, gpointer), gpointer, GDestroyNotify);
void g_io_channel_unref(GIOChannel *);
GDir *g_dir_open(const char *, guint, void *);
const char *g_dir_read_name(GDir *);
void g_dir_close(GDir *);
int open(const char *, int, ...);
int ioctl(int, unsigned long, ...);
gboolean gtk_window_has_toplevel_focus(void *);
GString *g_string_new(const char *);
char *g_string_free(GString *, gboolean);
GString *g_string_append_len(GString *, const char *, int);
GString *g_string_erase(GString *, int, int);
GHashTable *g_hash_table_new(void *, void *);
gpointer g_hash_table_lookup(GHashTable *, const void *);
void g_hash_table_insert(GHashTable *, gpointer, gpointer);
gboolean g_hash_table_remove(GHashTable *, const void *);
void g_hash_table_destroy(GHashTable *);

GtkWidget *gtk_scrolled_window_get_vscrollbar(void *);
int g_strcmp0(const char *, const char *);
void gtk_layout_get_size(void *, guint *, guint *);

const char *gtk_label_get_label(void *);
