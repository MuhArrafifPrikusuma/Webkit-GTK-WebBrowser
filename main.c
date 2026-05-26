#include "gio/gio.h"
#include <gtk/gtk.h>
#include <webkit/webkit.h>

static void activate(GtkApplication *app, gpointer user_data) {
  // create new window
  GtkWidget *window = gtk_application_window_new(app);
  // set window title of the window
  gtk_window_set_title(GTK_WINDOW(window), "HomePage");
  // set the default size of the window
  gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);

  // initialize webview to render the page
  GtkWidget *web_view = webkit_web_view_new();

  // Load a URL
  webkit_web_view_load_uri(WEBKIT_WEB_VIEW(web_view), "https://www.google.com");

  // Add the web view directly to the window
  gtk_window_set_child(GTK_WINDOW(window), web_view);

  // Present the window
  gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
  // initialize GtkApplication pointer variable
  GtkApplication *app;
  int status;
  char *my_data = "It WORKED!";

  app = gtk_application_new("org.expample.webkit", G_APPLICATION_NON_UNIQUE);
  // hooks up event listener (signal) from activate to app and G_CALLBACK is
  // used to connect to the signal and NULL should be replaced later on
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
  // this is the main event loop, instead of moving down to the next line of
  // code this loop will repeat forever until the user does any input.
  // G_APPLICATION() is a safe typecast validate app and treats it like is
  // parent class and assign all of this to status so that it will return
  // whatever the g_application_run return
  status = g_application_run(G_APPLICATION(app), argc, argv);
  // this is similar to free(). It deference app and freed it from the memory
  g_object_unref(app);
  // return exit status code
  return status;
}
