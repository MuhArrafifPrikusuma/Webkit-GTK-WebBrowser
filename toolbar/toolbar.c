#include "toolbar.h"
#include "../search/search.h"
#include "glib-object.h"
#include "glib.h"
#include "glibconfig.h"
#include "gtk/gtk.h"
#include "gtk/gtkshortcut.h"

// well what would you expect ofc this is just going to take you to the previous
// uri
static void onBack(GtkButton *btn, gpointer userData) {
  AppState *state = userData;

  webkit_web_view_go_back(WEBKIT_WEB_VIEW(state->webView));
}

// the opposite of above
static void onForward(GtkButton *btn, gpointer userData) {
  AppState *state = userData;

  webkit_web_view_go_forward(WEBKIT_WEB_VIEW(state->webView));
}

// refresh page
static void onRefresh(GtkButton *btn, gpointer userData) {
  AppState *state = userData;

  webkit_web_view_reload(WEBKIT_WEB_VIEW(state->webView));
}

GtkWidget *makeToolbar(AppState *state) {
  // initiate toolbar
  GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_add_css_class(toolbar, "toolbar");
  gtk_widget_set_margin_start(toolbar, 4);
  gtk_widget_set_margin_end(toolbar, 4);
  gtk_widget_set_margin_bottom(toolbar, 4);
  gtk_widget_set_margin_top(toolbar, 4);

  // do not expand horizontally
  gtk_widget_set_hexpand(toolbar, FALSE);

  // push those buttons to the very end
  GtkWidget *navRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_widget_set_halign(navRow, GTK_ALIGN_END);

  // create buttons widgets
  GtkWidget *backButton = gtk_button_new_from_icon_name("go-previous-symbolic");
  GtkWidget *forwardButton = gtk_button_new_from_icon_name("go-next-symbolic");
  GtkWidget *refreshButton =
      gtk_button_new_from_icon_name("view-refresh-symbolic");

  gtk_widget_add_css_class(backButton, "nav-btn");
  gtk_widget_add_css_class(forwardButton, "nav-btn");
  gtk_widget_add_css_class(refreshButton, "nav-btn");

  // connect it to the functions above
  g_signal_connect(backButton, "clicked", G_CALLBACK(onBack), state);
  g_signal_connect(forwardButton, "clicked", G_CALLBACK(onForward), state);
  g_signal_connect(refreshButton, "clicked", G_CALLBACK(onRefresh), state);

  // append everything
  gtk_box_append(GTK_BOX(navRow), backButton);
  gtk_box_append(GTK_BOX(navRow), forwardButton);
  gtk_box_append(GTK_BOX(navRow), refreshButton);

  // place the searchbar on the toolbar too
  GtkWidget *searchBar = makeUriSearch(state);
  gtk_box_append(GTK_BOX(toolbar), navRow);
  gtk_box_append(GTK_BOX(toolbar), searchBar);

  // set retrieves searchBar to use later or maybe not using it late
  g_object_set_data(G_OBJECT(toolbar), "search-bar", searchBar);

  return toolbar;
}
