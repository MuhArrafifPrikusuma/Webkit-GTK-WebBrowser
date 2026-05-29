#include "gdk/gdk.h"
#include "gio/gio.h"
#include "glib-object.h"
#include "glib.h"
#include "glibconfig.h"
#include "gtk/gtkcssprovider.h"
#include "jsc/jsc.h"
#include "pango/pango-layout.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <webkit/webkit.h>

#define TAB_CACHE_DIR "/tmp/browser_tabs"

/*
 * scrollY: store scroll position
 * webView: Tracks tab's URI
 * tabRow: contain label and favicon
 * tabLabel: pointer to GtkLabel inside row
 * TODO: display the actual favicon instead of placeholder
 * favicon: pointer to GtkImage to show favicon
 */
typedef struct {
  char *uri;
  char *title;
  double scrollY;
  GtkWidget *tabRow;
  GtkLabel *tabLabel;
  GtkImage *favicon;
  int id;
} Tab;

/*
 * tabList: Hold all tabs rows
 * webView: render websites
 * tabs: Glib dynamic arrays to store tabs which start from pointer at index 0
 * active: integer index of currently selected tab
 * nextTabId; increment id
 */
typedef struct {
  GtkWidget *tabList;
  GtkWidget *webView;
  GPtrArray *tabs;
  int active;
  int nextTabId;
} AppState;

static GtkWidget *makeTabRow(AppState *state, int index);
static void switchTab(AppState *state, int index);
static void addNewTab(AppState *state, const char *uri);

static char *tabCachPath(int id) {
  return g_strdup_printf("%s/%d.tab", TAB_CACHE_DIR, id);
}

static void saveToDisk(Tab *tab) {
  if (!tab->uri)
    return;
  g_mkdir_with_parents(TAB_CACHE_DIR, 0700);
  char *path = tabCachPath(tab->id);
  char *content = g_strdup_printf("%s\n%s\n%.2lf\n", tab->uri,
                                  tab->title ? tab->title : "", tab->scrollY);
  g_file_set_contents(path, content, -1, NULL);
  g_free(content);
  g_free(path);
}

static void loadFromDisk(Tab *tab) {
  char *path = tabCachPath(tab->id);
  char *content = NULL;
  if (!g_file_get_contents(path, &content, NULL, NULL)) {
    g_free(path);
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
  g_strfreev(lines);
  g_free(content);
  g_free(path);
}

static void deleteTabFromDisk(int id) {
  char *path = tabCachPath(id);
  remove(path);
  g_free(path);
}

typedef struct {
  AppState *state;
  int tabIndex;
} ScrollSaveData;

static void onScrollYResult(GObject *wv, GAsyncResult *result,
                            gpointer userData) {
  ScrollSaveData *d = userData;
  JSCValue *value = webkit_web_view_evaluate_javascript_finish(
      WEBKIT_WEB_VIEW(wv), result, NULL);
  if (value && jsc_value_is_number(value)) {
    Tab *tab = g_ptr_array_index(d->state->tabs, d->tabIndex);
    tab->scrollY = jsc_value_to_double(value);
    saveToDisk(tab);

    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(wv), "about:blank");
  }
  if (value)
    g_object_unref(value);
  g_free(d);
}

static void onUriChange(WebKitWebView *wv, GParamSpec *ps, gpointer userData) {
  AppState *state = userData;
  const char *uri = webkit_web_view_get_uri(wv);
  if (!uri || g_strcmp0(uri, "about:blank") == 0)
    return;
  Tab *tab = g_ptr_array_index(state->tabs, state->active);
  g_free(tab->uri);
  tab->uri = g_strdup(uri);
}

static void onTitleChange(WebKitWebView *wv, GParamSpec *ps,
                          gpointer userData) {
  AppState *state = userData;
  const char *title = webkit_web_view_get_title(wv);
  if (!title)
    return;
  Tab *tab = g_ptr_array_index(state->tabs, state->active);
  g_free(tab->title);
  tab->title = g_strdup(title);
  gtk_label_set_text(tab->tabLabel, title);
  saveToDisk(tab);
}

typedef struct {
  AppState *state;
  int targetIndex;
} SwitchData;

static void afterSaveSwitch(GObject *wv, GAsyncResult *result,
                            gpointer userData) {
  SwitchData *d = userData;
  AppState *state = d->state;
  int index = d->targetIndex;
  g_free(d);

  JSCValue *value = webkit_web_view_evaluate_javascript_finish(
      WEBKIT_WEB_VIEW(wv), result, NULL);
  if (value && jsc_value_is_number(value)) {
    Tab *old = g_ptr_array_index(state->tabs, state->active);
    old->scrollY = jsc_value_to_double(value);
    saveToDisk(old);
  }
  if (value)
    g_object_unref(value);

  Tab *old = g_ptr_array_index(state->tabs, state->active);
  gtk_widget_remove_css_class(old->tabRow, "active-tab");

  state->active = index;
  Tab *tab = g_ptr_array_index(state->tabs, index);
  gtk_widget_add_css_class(tab->tabRow, "active-tab");

  loadFromDisk(tab);
  gtk_label_set_text(tab->tabLabel, tab->title ? tab->title : "New Tab");

  if (tab->uri && g_strcmp0(tab->uri, "about:blank") != 0) {
    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(wv), tab->uri);
  }
}

static void switchTab(AppState *state, int index) {
  if (index == state->active && state->tabs->len > 1)
    return;

  SwitchData *d = g_new(SwitchData, 1);
  d->state = state;
  d->targetIndex = index;
  webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(state->webView),
                                      "window.scrollY", -1, NULL, NULL, NULL,
                                      afterSaveSwitch, d);
}

static void onLoadChange(WebKitWebView *wv, WebKitLoadEvent event,
                         gpointer userData) {
  AppState *state = userData;
  if (event != WEBKIT_LOAD_FINISHED)
    return;
  Tab *tab = g_ptr_array_index(state->tabs, state->active);

  if (tab->scrollY <= 0)
    return;
  char *js = g_strdup_printf("window.scrollTo(0, %.2lf);", tab->scrollY);
  webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(wv), js, -1, NULL, NULL,
                                      NULL, NULL, NULL);
  g_free(js);
}

typedef struct {
  AppState *state;
  int index;
} TabClickData;

static void onTabClick(GtkGestureClick *gesture, int nPress, double x, double y,
                       gpointer userData) {
  TabClickData *d = userData;
  switchTab(d->state, d->index);
}

static GtkWidget *makeTabRow(AppState *state, int index) {
  Tab *tab = g_ptr_array_index(state->tabs, index);

  GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
  gtk_widget_set_margin_start(row, 8);
  gtk_widget_set_margin_end(row, 8);
  gtk_widget_set_margin_top(row, 8);
  gtk_widget_set_margin_bottom(row, 8);

  tab->favicon = GTK_IMAGE(gtk_image_new_from_icon_name("text-html"));
  gtk_image_set_pixel_size(tab->favicon, 16);
  gtk_box_append(GTK_BOX(row), GTK_WIDGET(tab->favicon));

  tab->tabLabel = GTK_LABEL(gtk_label_new("New Tab"));
  gtk_label_set_ellipsize(tab->tabLabel, PANGO_ELLIPSIZE_END);
  gtk_label_set_xalign(tab->tabLabel, 0.0f);

  gtk_label_set_width_chars(tab->tabLabel, 5);
  gtk_label_set_max_width_chars(tab->tabLabel, 15);

  gtk_widget_set_hexpand(GTK_WIDGET(tab->tabLabel), FALSE);
  gtk_box_append(GTK_BOX(row), GTK_WIDGET(tab->tabLabel));

  GtkGesture *click = gtk_gesture_click_new();
  TabClickData *d = g_new(TabClickData, 1);
  d->state = state;
  d->index = index;
  g_signal_connect(click, "pressed", G_CALLBACK(onTabClick), d);
  gtk_widget_add_controller(row, GTK_EVENT_CONTROLLER(click));

  gtk_widget_set_cursor_from_name(row, "pointer");

  tab->tabRow = row;
  return row;
}

static void addNewTab(AppState *state, const char *uri) {
  Tab *tab = g_new0(Tab, 1);
  tab->uri = g_strdup(uri && g_strcmp0(uri, "about:blank") != 0
                          ? uri
                          : "https://search.brave.com");
  tab->title = g_strdup("New Tab");
  tab->id = state->nextTabId++;

  saveToDisk(tab);

  int index = state->tabs->len;
  g_ptr_array_add(state->tabs, tab);

  GtkWidget *row = makeTabRow(state, index);
  gtk_box_append(GTK_BOX(state->tabList), row);

  switchTab(state, index);
}

static void onMouseMotion(GtkEventControllerMotion *motion, double x, double y,
                          gpointer userData) {
  GtkRevealer *revealer = GTK_REVEALER(userData);
  gboolean isOpen = gtk_revealer_get_reveal_child(revealer);
  if (x <= 20) {
    gtk_revealer_set_reveal_child(revealer, TRUE);
  } else if (isOpen && x >= 200) {
    gtk_revealer_set_reveal_child(revealer, FALSE);
  }
}

static void onMouseLeave(GtkEventControllerMotion *motion, gpointer userData) {
  GtkRevealer *revealer = GTK_REVEALER(userData);
  gtk_revealer_set_reveal_child(revealer, FALSE);
}

static void onNewTab(GtkButton *btn, gpointer userData) {
  addNewTab(userData, "about:blank");
}

static void loadCSS(void) {
  GtkCssProvider *provider = gtk_css_provider_new();
  GFile *file = g_file_new_for_path("style.css");
  gtk_css_provider_load_from_file(provider, file);
  gtk_style_context_add_provider_for_display(
      gdk_display_get_default(), GTK_STYLE_PROVIDER(provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}
static void activate(GtkApplication *app, gpointer userData) {
  loadCSS();

  GtkWidget *window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "WebView");
  gtk_window_set_default_size(GTK_WINDOW(window), 1200, 700);

  AppState *state = g_new0(AppState, 1);
  state->tabs = g_ptr_array_new();
  state->active = 0;
  state->nextTabId = 0;

  g_mkdir_with_parents(TAB_CACHE_DIR, 0700);

  GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_set_size_request(sidebar, 190, -1);
  gtk_widget_set_hexpand(sidebar, FALSE);
  gtk_widget_add_css_class(sidebar, "sidebar");
  gtk_widget_set_margin_bottom(sidebar, 0);
  gtk_widget_set_margin_top(sidebar, 0);

  state->tabList = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  gtk_box_append(GTK_BOX(sidebar), state->tabList);

  GtkWidget *newTabBtn = gtk_button_new_with_label("+ New Tab");
  gtk_widget_add_css_class(newTabBtn, "new-tab-btn");
  gtk_widget_set_margin_start(newTabBtn, 8);
  gtk_widget_set_margin_end(newTabBtn, 8);
  gtk_widget_set_halign(newTabBtn, GTK_ALIGN_START);
  gtk_box_append(GTK_BOX(sidebar), newTabBtn);

  state->webView = webkit_web_view_new();
  g_signal_connect(state->webView, "notify::uri", G_CALLBACK(onUriChange),
                   state);
  g_signal_connect(state->webView, "notify::title", G_CALLBACK(onTitleChange),
                   state);
  g_signal_connect(state->webView, "load-change", G_CALLBACK(onLoadChange),
                   state);

  GtkWidget *revealer = gtk_revealer_new();
  gtk_revealer_set_transition_type(GTK_REVEALER(revealer),
                                   GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT);
  gtk_revealer_set_transition_duration(GTK_REVEALER(revealer), 200);

  gtk_revealer_set_child(GTK_REVEALER(revealer), sidebar);
  gtk_revealer_set_reveal_child(GTK_REVEALER(revealer), FALSE);
  gtk_widget_set_valign(revealer, GTK_ALIGN_FILL);
  gtk_widget_set_halign(revealer, GTK_ALIGN_START);

  GtkWidget *overlay = gtk_overlay_new();
  gtk_overlay_set_child(GTK_OVERLAY(overlay), state->webView);
  gtk_overlay_add_overlay(GTK_OVERLAY(overlay), revealer);
  gtk_overlay_set_measure_overlay(GTK_OVERLAY(overlay), revealer, FALSE);
  gtk_overlay_set_clip_overlay(GTK_OVERLAY(overlay), revealer, TRUE);

  GtkEventController *motionCtrl = gtk_event_controller_motion_new();
  g_signal_connect(motionCtrl, "motion", G_CALLBACK(onMouseMotion), revealer);
  g_signal_connect(motionCtrl, "leave", G_CALLBACK(onMouseLeave), revealer);
  gtk_widget_add_controller(overlay, motionCtrl);

  g_signal_connect(newTabBtn, "clicked", G_CALLBACK(onNewTab), state);

  addNewTab(state, "https://search.brave.com");

  gtk_window_set_child(GTK_WINDOW(window), overlay);
  gtk_window_present(GTK_WINDOW(window));
}
int main(int argc, char **argv) {
  GtkApplication *app;
  int status;
  app = gtk_application_new("org.webview.app", G_APPLICATION_NON_UNIQUE);
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
  status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}
