#include "gio/gio.h"
#include "glib-object.h"
#include "glib.h"
#include "glibconfig.h"
#include "gtk/gtkshortcut.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <webkit/webkit.h>

static void on_mouse_enter(GtkEventControllerMotion *controller, double x,
                           double y, gpointer userData) {
  GtkRevealer *reveal = GTK_REVEALER(userData);
  gtk_revealer_set_reveal_child(reveal, true);
}
static void on_mouse_leavel(GtkEventControllerMotion *controller, double x,
                            double y, gpointer userData) {
  GtkRevealer *reveal = GTK_REVEALER(userData);
  gtk_revealer_set_reveal_child(reveal, FALSE);
}
static void activate(GtkApplication *app, gpointer isSuccess) {
  GtkWidget *window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "webview");
  gtk_window_set_default_size(GTK_WINDOW(window), 1200, 800);
  GtkWidget *notebook = gtk_notebook_new();
  GtkWidget *webview = webkit_web_view_new();
  gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_LEFT);
  webkit_web_view_load_uri(WEBKIT_WEB_VIEW(webview), "https://www.google.com");
  GtkWidget *label1 = gtk_label_new("google");
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), webview, label1);
  gtk_window_set_child(GTK_WINDOW(window), notebook);
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
