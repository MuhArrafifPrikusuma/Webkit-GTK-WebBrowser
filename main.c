#include "gio/gio.h"
#include "glib-object.h"
#include "glib.h"
#include "glibconfig.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <webkit/webkit.h>
static void onMouseMotion(GtkEventControllerMotion *motion, double x, double y,
                          gpointer userData) {
  GtkRevealer *revealer = GTK_REVEALER(userData);
  gboolean isOpen = gtk_revealer_get_reveal_child(revealer);

  if (x <= 15) {
    gtk_revealer_set_reveal_child(revealer, TRUE);
  } else if (isOpen && x >= 250) {
    gtk_revealer_set_reveal_child(GTK_REVEALER(revealer), FALSE);
  }
}
static void onMouseLeave(GtkEventControllerMotion *motion, gpointer userData) {
  GtkRevealer *revealer = GTK_REVEALER(userData);
  gtk_revealer_set_reveal_child(revealer, FALSE);
}

static void activate(GtkApplication *app, gpointer userData) {
  GtkWidget *window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "WebView");
  gtk_window_set_default_size(GTK_WINDOW(window), 1200, 700);

  GtkWidget *webView = webkit_web_view_new();
  webkit_web_view_load_uri(WEBKIT_WEB_VIEW(webView), "https://www.google.com");

  GtkWidget *notebook = gtk_notebook_new();
  gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
  gtk_widget_set_size_request(notebook, 240, -1);

  GtkWidget *tabLabel = gtk_label_new("Home");
  GtkWidget *emptyPage = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), webView, tabLabel);

  GtkWidget *revealer = gtk_revealer_new();
  gtk_revealer_set_transition_duration(GTK_REVEALER(revealer), 200);
  gtk_revealer_set_transition_type(GTK_REVEALER(revealer),
                                   GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT);
  gtk_revealer_set_child(GTK_REVEALER(revealer), notebook);
  gtk_revealer_set_reveal_child(GTK_REVEALER(revealer), FALSE);
  gtk_widget_set_valign(revealer, GTK_ALIGN_FILL);
  gtk_widget_set_halign(revealer, GTK_ALIGN_START);

  GtkWidget *overlay = gtk_overlay_new();

  gtk_overlay_set_child(GTK_OVERLAY(overlay), webView);
  gtk_overlay_add_overlay(GTK_OVERLAY(overlay), revealer);

  gtk_overlay_set_measure_overlay(GTK_OVERLAY(overlay), revealer, FALSE);
  gtk_overlay_set_clip_overlay(GTK_OVERLAY(overlay), revealer, TRUE);

  GtkEventController *motionCtrl = gtk_event_controller_motion_new();
  g_signal_connect(motionCtrl, "motion", G_CALLBACK(onMouseMotion), revealer);
  g_signal_connect(motionCtrl, "leave", G_CALLBACK(onMouseLeave), revealer);
  gtk_widget_add_controller(overlay, motionCtrl);

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
