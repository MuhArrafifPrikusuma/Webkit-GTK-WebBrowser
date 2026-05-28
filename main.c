#include "gdk/gdk.h"
#include "gio/gio.h"
#include "glib-object.h"
#include "glib.h"
#include "glibconfig.h"
#include "gtk/gtkcssprovider.h"
#include "pango/pango-layout.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <webkit/webkit.h>

typedef struct {
  WebKitWebView *webView;
  GtkWidget *tabRow;
  GtkLabel *tabLabel;
  GtkImage *favicon;
} Tab;

typedef struct {
  GtkWidget *tabList;
  GtkWidget *webView;
  GPtrArray *tabs;
  int active;
} AppState;

static GtkWidget *makeTabRow(AppState *state, int index);
static void switchTab(AppState *state, int index);
static void addNewTab(AppState *state, const char *uri);

static void onTitleChange(WebKitWebView *wv, GParamSpec *ps,
                          gpointer userData) {
  Tab *tab = userData;
  const char *title = webkit_web_view_get_title(wv);
  gtk_label_set_text(tab->tabLabel, title ? title : "loading...");
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

static void onNewTab(GtkButton *btn, gpointer userData) {
  AppState *state = userData;
  addNewTab(state, "about:blank");
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
  gtk_widget_set_hexpand(GTK_WIDGET(tab->tabLabel), TRUE);
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

static void switchTab(AppState *state, int index) {
  Tab *old = g_ptr_array_index(state->tabs, state->active);
  gtk_widget_remove_css_class(old->tabRow, "active-tab");

  state->active = index;

  Tab *tab = g_ptr_array_index(state->tabs, index);
  gtk_widget_add_css_class(tab->tabRow, "active-tab");

  const char *uri = webkit_web_view_get_uri(tab->webView);
  if (uri)
    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(state->webView), uri);
}

static void addNewTab(AppState *state, const char *uri) {
  Tab *tab = g_new0(Tab, 1);

  tab->webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
  webkit_web_view_load_uri(tab->webView, uri);
  g_signal_connect(tab->webView, "notify::title", G_CALLBACK(onTitleChange),
                   tab);

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
  } else if (isOpen && x >= 250) {
    gtk_revealer_set_reveal_child(revealer, FALSE);
  }
}

static void onMouseLeave(GtkEventControllerMotion *motion, gpointer userData) {
  GtkRevealer *revealer = GTK_REVEALER(userData);
  gtk_revealer_set_reveal_child(revealer, FALSE);
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

  GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_set_size_request(sidebar, 240, -1);
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
  webkit_web_view_load_uri(WEBKIT_WEB_VIEW(state->webView),
                           "https://search.brave.com");

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

  g_signal_connect(newTabBtn, "clicked", G_CALLBACK(addNewTab), state);

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
