#include "sidebar.h"
#include "../tabs/tabs.h"
#include <gtk/gtk.h>

void onMouseMotion(GtkEventControllerMotion *motion, double x, double y,
                   gpointer userData) {
  GtkRevealer *revealer = GTK_REVEALER(userData);
  gboolean isOpen = gtk_revealer_get_reveal_child(revealer);
  if (x <= 20) {
    gtk_revealer_set_reveal_child(revealer, TRUE);
  } else if (isOpen && x >= 200) {
    gtk_revealer_set_reveal_child(revealer, FALSE);
  }
}

void onMouseLeave(GtkEventControllerMotion *motion, gpointer userData) {
  GtkRevealer *revealer = GTK_REVEALER(userData);
  gtk_revealer_set_reveal_child(revealer, FALSE);
}

// wrapper to call addNewTab so the data doesn't get passed directly and
// misinterpreted as AppState* which will cause segfault
void onNewTab(GtkButton *btn, gpointer userData) {
  addNewTab(userData, "about:blank");
}
