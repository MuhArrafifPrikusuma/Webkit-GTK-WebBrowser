#include "tabs.h"
#include "cairo.h"
#include "gdk/gdk.h"
#include "glib-object.h"
#include "glib.h"
#include "glibconfig.h"
#include "gtk/gtkrevealer.h"
#include "gtk/gtkshortcut.h"
#include "jsc/jsc.h"
#include "pango/pango-layout.h"
#include <cairo.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>

// automatically create buffer space for cache path
char *tabCachePath(int id) {
  return g_strdup_printf("%s/%d.tab", TAB_CACHE_DIR, id);
}

// write current tab state to disk
void saveToDisk(Tab *tab) {
  if (!tab->uri)
    return;
  // 0700 octal permission ensuring only owner has access to it
  g_mkdir_with_parents(TAB_CACHE_DIR, 0700);
  char *path = tabCachePath(tab->id);
  char *content = g_strdup_printf("%s\n%s\n%.2lf\n", tab->uri,
                                  tab->title ? tab->title : "", tab->scrollY);
  // -1 mean use strlen to determine lenght automatically
  // write string to disk automatically
  g_file_set_contents(path, content, -1, NULL);
  g_free(content);
  g_free(path);
}

void loadFromDisk(Tab *tab) {
  char *path = tabCachePath(tab->id);
  char *content = NULL;
  // check if file does not exist
  if (!g_file_get_contents(path, &content, NULL, NULL)) {
    g_free(path);
    return;
  }

  char **lines = g_strsplit(content, "\n", 4);
  if (lines[0]) {
    g_free(tab->uri);
    tab->uri = g_strdup(lines[0]);
  }
  if (lines[1]) {
    g_free(tab->title);
    tab->title = g_strdup(lines[1]);
  }
  if (lines[2]) {
    tab->scrollY = g_ascii_strtod(lines[2], NULL);
  }
  // free an entire array and every string in it
  g_strfreev(lines);
  g_free(content);
  g_free(path);
}

// TODO: use this on tab delete
void deleteTabFromDisk(int id) {
  char *path = tabCachePath(id);
  remove(path);
  g_free(path);
}

static void onRowEnter(GtkEventControllerMotion *motion, double x, double y,
                       gpointer userData) {
  GtkRevealer *revealer = GTK_REVEALER(userData);
  GtkWidget *overlay =
      gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(motion));
  gboolean isOpen = gtk_revealer_get_reveal_child(revealer);
  int overlayWidth = gtk_widget_get_width(overlay);
  if (x >= overlayWidth - 30) {
    gtk_revealer_set_reveal_child(revealer, TRUE);
  } else {
    gtk_revealer_set_reveal_child(revealer, FALSE);
  }
}

static void onRowLeave(GtkEventControllerMotion *motion, gpointer userData) {
  GtkRevealer *revealer = GTK_REVEALER(userData);

  gtk_revealer_set_reveal_child(revealer, FALSE);
}

// FIX: this shit doesn't work
// static void onFaviconChange(WebKitWebView *webView, GParamSpec *ps,
//                            gpointer userData) {
//  AppState *state = userData;
//  if (state->active < 0 || state->active >= state->tabs->len)
//    return;
//
//  Tab *tab = g_ptr_array_index(state->tabs, state->active);
//  cairo_surface_t *surface =
//      (cairo_surface_t *)webkit_web_view_get_favicon(webView);
//
//  if (surface && cairo_surface_get_type(surface) == CAIRO_SURFACE_TYPE_IMAGE)
//  {
//    int width = cairo_image_surface_get_width(surface);
//    int height = cairo_image_surface_get_height(surface);
//    int stride = cairo_image_surface_get_stride(surface);
//    unsigned char *data = cairo_image_surface_get_data(surface);
//
//    if (width > 0 && height > 0 && data != NULL) {
//      GBytes *bytes = g_bytes_new_with_free_func(
//          data, height * stride, (GDestroyNotify)cairo_surface_destroy,
//          cairo_surface_reference(surface));
//      GdkTexture *texture = gdk_memory_texture_new(
//          width, height, GDK_MEMORY_DEFAULT, bytes, stride);
//      if (texture) {
//        gtk_image_set_from_paintable(tab->favicon, GDK_PAINTABLE(texture));
//        gtk_image_set_pixel_size(tab->favicon, 16);
//
//        g_object_unref(texture);
//      }
//      g_bytes_unref(bytes);
//    }
//  }
//}

/*
 * TABS UI CREATION
 */
typedef struct {
  AppState *state;
  int index;
} TabClickData;

// simply detect if any tab is pressed, sends data to switch tab and initiate
// the switch
static void onTabClick(GtkGestureClick *gesture, int nPress, double x, double y,
                       gpointer userData) {
  TabClickData *d = userData;
  switchTab(d->state, d->index);
}

GtkWidget *makeTabRow(AppState *state, int index) {
  Tab *tab = g_ptr_array_index(state->tabs, index);

  // create a new box for the tab
  GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_widget_set_margin_start(row, 8);
  gtk_widget_set_margin_end(row, 8);
  gtk_widget_set_margin_top(row, 4);
  gtk_widget_set_margin_bottom(row, 4);
  gtk_widget_set_halign(row, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand(row, FALSE);

  // TODO: load the actualy favicon instead of placeholder
  tab->favicon = GTK_IMAGE(gtk_image_new_from_icon_name("text-html"));
  gtk_image_set_pixel_size(tab->favicon, 16);
  gtk_box_append(GTK_BOX(row), GTK_WIDGET(tab->favicon));

  // FIX:
  //  g_signal_connect(state->webView, "notify::favicon",
  // Set the label to new tab and use PANGO_ELLIPSIZE_END to truncate text with

  // ... if text is abov max chars
  tab->tabLabel = GTK_LABEL(gtk_label_new("New Tab"));
  gtk_label_set_ellipsize(tab->tabLabel, PANGO_ELLIPSIZE_END);
  gtk_label_set_xalign(tab->tabLabel, 0.0f);
  gtk_label_set_width_chars(tab->tabLabel, 5);
  gtk_label_set_max_width_chars(tab->tabLabel, 12);
  // make sure it doesn't go beyond limit and append it to row
  gtk_widget_set_hexpand(GTK_WIDGET(tab->tabLabel), FALSE);
  gtk_box_append(GTK_BOX(row), GTK_WIDGET(tab->tabLabel));

  // TODO: make it actually work and fix the row styling, IS GARBAAGE
  tab->closeBtn = gtk_button_new_from_icon_name("window-close-symbolic");
  gtk_widget_add_css_class(tab->closeBtn, "close-tab");
  gtk_image_set_pixel_size(
      GTK_IMAGE(gtk_button_get_child(GTK_BUTTON(tab->closeBtn))), 12);

  gtk_widget_set_halign(tab->closeBtn, GTK_ALIGN_END);
  gtk_widget_set_valign(tab->closeBtn, GTK_ALIGN_CENTER);

  GtkWidget *revealer = gtk_revealer_new();
  gtk_revealer_set_transition_type(GTK_REVEALER(revealer),
                                   GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT);
  gtk_revealer_set_transition_duration(GTK_REVEALER(revealer), 200);
  gtk_revealer_set_child(GTK_REVEALER(revealer), tab->closeBtn);
  gtk_revealer_set_reveal_child(GTK_REVEALER(revealer), FALSE);
  gtk_widget_set_halign(revealer, GTK_ALIGN_END);
  gtk_widget_set_valign(revealer, GTK_ALIGN_CENTER);

  GtkWidget *overlay = gtk_overlay_new();
  gtk_widget_add_css_class(row, "tab-row");
  gtk_overlay_set_child(GTK_OVERLAY(overlay), row);
  gtk_overlay_add_overlay(GTK_OVERLAY(overlay), revealer);
  gtk_overlay_set_measure_overlay(GTK_OVERLAY(overlay), revealer, FALSE);
  gtk_overlay_set_clip_overlay(GTK_OVERLAY(overlay), revealer, FALSE);

  GtkEventController *hoverctrl = gtk_event_controller_motion_new();
  g_signal_connect(hoverctrl, "motion", G_CALLBACK(onRowEnter), revealer);
  g_signal_connect(hoverctrl, "leave", G_CALLBACK(onRowLeave), revealer);
  gtk_widget_add_controller(overlay, hoverctrl);

  // trigger click on row area
  GtkGesture *click = gtk_gesture_click_new();
  // to HEAP!
  TabClickData *d = g_new(TabClickData, 1);
  d->state = state;
  d->index = index;
  g_signal_connect_data(click, "pressed", G_CALLBACK(onTabClick), d,
                        (GClosureNotify)g_free, G_CONNECT_DEFAULT);
  gtk_widget_add_controller(row, GTK_EVENT_CONTROLLER(click));
  // cursor show hand icon
  gtk_widget_set_cursor_from_name(overlay, "pointer");

  tab->tabRow = overlay;
  return overlay;
}

typedef struct {
  AppState *state;
  int targetIndex;
} SwitchData;

// save scroll value to disk, update active index, load new title and uri
static void afterSaveSwitch(GObject *wv, GAsyncResult *result,
                            gpointer userData) {
  SwitchData *d = userData;
  AppState *state = d->state;
  int index = d->targetIndex;
  g_free(d);

  //  saving scrollY value to disk
  JSCValue *value = webkit_web_view_evaluate_javascript_finish(
      WEBKIT_WEB_VIEW(wv), result, NULL);
  if (value && jsc_value_is_number(value)) {
    Tab *old = g_ptr_array_index(state->tabs, state->active);
    old->scrollY = jsc_value_to_double(value);
    saveToDisk(old);
  }
  if (value)
    g_object_unref(value);

  // unhighlight old inactive tab
  Tab *old = g_ptr_array_index(state->tabs, state->active);
  gtk_widget_remove_css_class(old->tabRow, "active-tab");

  // update active tab and highlight it
  state->active = index;
  Tab *tab = g_ptr_array_index(state->tabs, index);
  gtk_widget_add_css_class(tab->tabRow, "active-tab");

  // load and take uri and title from tmp file and assigning the lable to the
  // loaded title or New Tab if there is no loaded title
  loadFromDisk(tab);
  gtk_label_set_text(tab->tabLabel, tab->title ? tab->title : "New Tab");
  if (tab->uri && g_strcmp0(tab->uri, "about:blank") != 0)
    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(state->webView), tab->uri);
  // FIX: onFaviconChange(WEBKIT_WEB_VIEW(state->webView), NULL, state);
}

void switchTab(AppState *state, int index) {
  // if there is more than one tab open and current index is active then do
  // nothing
  if (index == state->active && state->tabs->len > 1)
    return;

  // allocate the struct to heap so it wouldn't be freed before callbacks
  SwitchData *d = g_new(SwitchData, 1);
  // assigning value to d
  d->state = state;
  d->targetIndex = index;
  // extract javascript scrollY position from webView
  webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(state->webView),
                                      "window.scrollY", -1, NULL, NULL, NULL,
                                      afterSaveSwitch, d);
}

void addNewTab(AppState *state, const char *uri) {
  // g_new0 is just calloc but with automatic allocation
  Tab *tab = g_new0(Tab, 1);
  tab->uri = g_strdup("https://search.brave.com");
  tab->title = g_strdup("New Tab");
  tab->id = state->nextTabId++;

  // write to disk immediately even before the page load
  saveToDisk(tab);

  // make sure it's equal to the new tab index before gptrarrayadd
  int index = state->tabs->len;
  // add pointer to tab at the end of tabs
  g_ptr_array_add(state->tabs, tab);

  // build a tab widget
  GtkWidget *row = makeTabRow(state, index);
  // make sure row is at the very bottom of the list
  gtk_box_append(GTK_BOX(state->tabList), row);

  // set newly build tab as the new active tab
  switchTab(state, index);
}
