#include "css/css.h"
#include "sidebar/sidebar.h"
#include "state.h"
#include "tabs/tabs.h"
#include "toolbar/toolbar.h"
#include "webview/webview.h"
#include <gtk/gtk.h>
#include <webkit/webkit.h>

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

  GtkWidget *toolbar = makeToolbar(state);
  gtk_box_append(GTK_BOX(sidebar), toolbar);

  // append tabList to sidebar
  state->tabList = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  gtk_box_append(GTK_BOX(sidebar), state->tabList);

  GtkWidget *newTabBtn = gtk_button_new_with_label("+ New Tab");
  gtk_widget_add_css_class(newTabBtn, "new-tab-btn");
  gtk_widget_set_margin_start(newTabBtn, 8);
  gtk_widget_set_margin_end(newTabBtn, 8);
  gtk_widget_set_halign(newTabBtn, GTK_ALIGN_FILL);
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
