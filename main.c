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
#include <string.h>
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

// automatically create buffer space for cache path
static char *tabCachPath(int id) {
  return g_strdup_printf("%s/%d.tab", TAB_CACHE_DIR, id);
}

// write current tab state to disk
static void saveToDisk(Tab *tab) {
  if (!tab->uri)
    return;
  // 0700 octal permission ensuring only owner has access to it
  g_mkdir_with_parents(TAB_CACHE_DIR, 0700);
  char *path = tabCachPath(tab->id);
  char *content = g_strdup_printf("%s\n%s\n%.2lf\n", tab->uri,
                                  tab->title ? tab->title : "", tab->scrollY);
  // -1 mean use strlen to determine lenght automatically
  // write string to disk automatically
  g_file_set_contents(path, content, -1, NULL);
  g_free(content);
  g_free(path);
}

static void loadFromDisk(Tab *tab) {
  char *path = tabCachPath(tab->id);
  char *content = NULL;
  // check if file does not exist
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
  // free an entire array and every string in it
  g_strfreev(lines);
  g_free(content);
  g_free(path);
}

// TODO: use this on tab delete
static void deleteTabFromDisk(int id) {
  char *path = tabCachPath(id);
  remove(path);
  g_free(path);
}

// pack AppState and tabIndex into one pointer
typedef struct {
  AppState *state;
  int tabIndex;
} ScrollSaveData;

static void onScrollYResult(GObject *wv, GAsyncResult *result,
                            gpointer userData) {
  // gpointer is generic untype pointer used to pass in callbacks so they can
  // access global scope in this case it's used to pass custom struct
  ScrollSaveData *d = userData;

  // retrieves the result of the async JS evaluation to JSCValue* which is a
  // GObject used to wrap javascript return value
  JSCValue *value = webkit_web_view_evaluate_javascript_finish(
      WEBKIT_WEB_VIEW(wv), result, NULL);

  // returned value MUST! be a number
  if (value && jsc_value_is_number(value)) {
    Tab *tab = g_ptr_array_index(d->state->tabs, d->tabIndex);
    // extract javascript number to C double
    tab->scrollY = jsc_value_to_double(value);
    saveToDisk(tab);

    // load a blank page to optimize memory
    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(wv), "about:blank");
  }
  if (value)
    g_object_unref(value);
  g_free(d);
}

// releases the old uri and copies the new one
static void onUriChange(WebKitWebView *wv, GParamSpec *ps, gpointer userData) {
  AppState *state = userData;
  const char *uri = webkit_web_view_get_uri(wv);

  if (!uri || g_strcmp0(uri, "about:blank") == 0)
    return;

  Tab *tab = g_ptr_array_index(state->tabs, state->active);

  g_free(tab->uri);
  tab->uri = g_strdup(uri);
}

// replaces the old title with a new one
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
  if (tab->uri && g_strcmp0(tab->uri, "about:blank") != 0) {
    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(wv), tab->uri);
  }
}

static void switchTab(AppState *state, int index) {
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

static void onLoadChange(WebKitWebView *wv, WebKitLoadEvent event,
                         gpointer userData) {
  AppState *state = userData;
  // don't return anything before page finish loading
  if (event != WEBKIT_LOAD_FINISHED)
    return;

  Tab *tab = g_ptr_array_index(state->tabs, state->active);
  if (tab->scrollY <= 0)
    return;

  // build javascript string to dynamically with the saved scrollY value on that
  // tab
  char *js = g_strdup_printf("window.scrollTo(0, %.2lf);", tab->scrollY);
  webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(wv), js, -1, NULL, NULL,
                                      NULL, NULL, NULL);
  g_free(js);
}
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

static GtkWidget *makeTabRow(AppState *state, int index) {
  Tab *tab = g_ptr_array_index(state->tabs, index);

  // FIX: hover style doesn't apply to margin but there is no fking padding
  // create a new box for the tab
  GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
  gtk_widget_set_margin_start(row, 8);
  gtk_widget_set_margin_end(row, 8);
  gtk_widget_set_margin_top(row, 8);
  gtk_widget_set_margin_bottom(row, 8);

  // TODO: load the actualy favicon instead of placeholder
  tab->favicon = GTK_IMAGE(gtk_image_new_from_icon_name("text-html"));
  gtk_image_set_pixel_size(tab->favicon, 16);
  gtk_box_append(GTK_BOX(row), GTK_WIDGET(tab->favicon));

  // Set the label to new tab and use PANGO_ELLIPSIZE_END to truncate text with
  // ... if text is abov max chars
  tab->tabLabel = GTK_LABEL(gtk_label_new("New Tab"));
  gtk_label_set_ellipsize(tab->tabLabel, PANGO_ELLIPSIZE_END);
  gtk_label_set_xalign(tab->tabLabel, 0.0f);
  gtk_label_set_width_chars(tab->tabLabel, 5);
  gtk_label_set_max_width_chars(tab->tabLabel, 15);
  // make sure it doesn't go beyond limit and append it to row
  gtk_widget_set_hexpand(GTK_WIDGET(tab->tabLabel), FALSE);
  gtk_box_append(GTK_BOX(row), GTK_WIDGET(tab->tabLabel));

  // trigger click on row area
  GtkGesture *click = gtk_gesture_click_new();
  // to HEAP!
  TabClickData *d = g_new(TabClickData, 1);
  d->state = state;
  d->index = index;
  g_signal_connect(click, "pressed", G_CALLBACK(onTabClick), d);
  gtk_widget_add_controller(row, GTK_EVENT_CONTROLLER(click));
  // cursor show hand icon
  gtk_widget_set_cursor_from_name(row, "pointer");

  tab->tabRow = row;
  return row;
}

static void addNewTab(AppState *state, const char *uri) {
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

// wrapper to call addNewTab so the data doesn't get passed directly and
// misinterpreted as AppState* which will cause segfault
static void onNewTab(GtkButton *btn, gpointer userData) {
  addNewTab(userData, "about:blank");
}

// accept nothing and return nothing to load CSS
static void loadCSS(void) {
  GtkCssProvider *provider = gtk_css_provider_new();
  GFile *file = g_file_new_for_path("style.css");

  gtk_css_provider_load_from_file(provider, file);
  // override default theme
  gtk_style_context_add_provider_for_display(
      gdk_display_get_default(), GTK_STYLE_PROVIDER(provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}
static void activate(GtkApplication *app, gpointer userData) {
  loadCSS();

  GtkWidget *window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "WebView");
  gtk_window_set_default_size(GTK_WINDOW(window), 1200, 700);

  // to Heap! again
  AppState *state = g_new0(AppState, 1);
  state->tabs = g_ptr_array_new();
  state->active = 0;
  state->nextTabId = 0;

  // create a directory
  g_mkdir_with_parents(TAB_CACHE_DIR, 0700);

  // create sidebar widget
  GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_set_size_request(sidebar, 190, -1);
  gtk_widget_set_hexpand(sidebar, FALSE);
  gtk_widget_add_css_class(sidebar, "sidebar");
  gtk_widget_set_margin_bottom(sidebar, 0);
  gtk_widget_set_margin_top(sidebar, 0);

  // append tabList to sidebar
  state->tabList = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  gtk_box_append(GTK_BOX(sidebar), state->tabList);

  GtkWidget *newTabBtn = gtk_button_new_with_label("+ New Tab");
  gtk_widget_add_css_class(newTabBtn, "new-tab-btn");
  gtk_widget_set_margin_start(newTabBtn, 8);
  gtk_widget_set_margin_end(newTabBtn, 8);
  gtk_widget_set_halign(newTabBtn, GTK_ALIGN_START);
  gtk_box_append(GTK_BOX(sidebar), newTabBtn);

  // keeps uri and title current and restore scroll position
  state->webView = webkit_web_view_new();
  g_signal_connect(state->webView, "notify::uri", G_CALLBACK(onUriChange),
                   state);
  g_signal_connect(state->webView, "notify::title", G_CALLBACK(onTitleChange),
                   state);
  g_signal_connect(state->webView, "load-change", G_CALLBACK(onLoadChange),
                   state);

  // reveal sidebar
  GtkWidget *revealer = gtk_revealer_new();
  gtk_revealer_set_transition_type(GTK_REVEALER(revealer),
                                   GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT);
  gtk_revealer_set_transition_duration(GTK_REVEALER(revealer), 200);

  gtk_revealer_set_child(GTK_REVEALER(revealer), sidebar);
  gtk_revealer_set_reveal_child(GTK_REVEALER(revealer), FALSE);
  gtk_widget_set_valign(revealer, GTK_ALIGN_FILL);
  gtk_widget_set_halign(revealer, GTK_ALIGN_START);

  // set webView at the very bottom of the overlay and revealer on top of it
  GtkWidget *overlay = gtk_overlay_new();
  gtk_overlay_set_child(GTK_OVERLAY(overlay), state->webView);
  gtk_overlay_add_overlay(GTK_OVERLAY(overlay), revealer);
  // don't change webView size
  gtk_overlay_set_measure_overlay(GTK_OVERLAY(overlay), revealer, FALSE);
  // click pass throught revealer and will only register to revealer if it's
  // visible
  gtk_overlay_set_clip_overlay(GTK_OVERLAY(overlay), revealer, TRUE);

  // captures mouse movement in overlay
  GtkEventController *motionCtrl = gtk_event_controller_motion_new();
  g_signal_connect(motionCtrl, "motion", G_CALLBACK(onMouseMotion), revealer);
  g_signal_connect(motionCtrl, "leave", G_CALLBACK(onMouseLeave), revealer);
  gtk_widget_add_controller(overlay, motionCtrl);

  // create a new tab on button click
  g_signal_connect(newTabBtn, "clicked", G_CALLBACK(onNewTab), state);
  // add the starting tab
  addNewTab(state, "https://search.brave.com");

  // set overlay as window content and make the window visible
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
