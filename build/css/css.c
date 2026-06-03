#include "css.h"
#include "glib-object.h"
#include <gtk/gtk.h>

// accept nothing and return nothing to load CSS
void loadCSS(void) {
  GtkCssProvider *provider = gtk_css_provider_new();
  GFile *file = g_file_new_for_path("style.css");

  gtk_css_provider_load_from_file(provider, file);
  gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                             GTK_STYLE_PROVIDER(provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_USER);
  g_object_unref(provider);
  g_object_unref(file);
}
