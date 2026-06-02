#include "search.h"
#include "glib-object.h"
#include "glib.h"
#include "glibconfig.h"
#include "gtk/gtk.h"
#include "gtk/gtkshortcut.h"
#include <stdio.h>
#include <string.h>

static gboolean isUri(const char *text) {
  if (strstr(text, "://"))
    return TRUE;
  if (g_str_has_prefix(text, "localhost"))
    return TRUE;

  if (strchr(text, '.') && !strchr(text, ' '))
    return TRUE;
  if (g_strcmp0(text, "about:blank") == 0)
    return TRUE;
  return FALSE;
}

static void navigate(AppState *state, const char *text) {
  if (!text || *text == '\0')
    return;

  char *uri;
  if (isUri(text)) {
    if (strstr(text, "://")) {
      uri = g_strdup(text);
    } else {
      uri = g_strdup_printf("https://");
    }
  } else if (g_strcmp0(text, "about:blank") == 0) {
    uri = g_strdup(text);
  } else {
    char *encode = g_uri_escape_string(text, NULL, FALSE);
    uri = g_strdup_printf("https://google.com/search?q=%s", encode);
    g_free(encode);
  }
  webkit_web_view_load_uri(WEBKIT_WEB_VIEW(state->webView), uri);
  g_free(uri);
}

static void onUriActive(GtkEntry *entry, gpointer userData) {
  AppState *state = userData;
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  navigate(state, text);

  gtk_widget_grab_focus(state->webView);
}

static void onSyncUri(WebKitWebView *wv, GParamSpec *ps, gpointer userData) {
  UriBarData *d = userData;
  const char *uri = webkit_web_view_get_uri(wv);
  if (!uri || g_strcmp0(uri, "about:blank") == 0)
    return;
  if (!gtk_widget_has_focus(d->entry))
    gtk_editable_set_text(GTK_EDITABLE(d->entry), uri);
}

static void onFocus(GtkEventControllerFocus *focus, gpointer userData) {
  GtkWidget *entry = GTK_WIDGET(userData);
  gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
}

void syncSearch(UriBarData *d, const char *uri) {
  if (!uri || g_strcmp0(uri, "about:blank") == 0)
    return;
  if (!gtk_widget_has_focus(d->entry))
    gtk_editable_set_text(GTK_EDITABLE(d->entry), uri);
}

GtkWidget *makeUriSearch(AppState *state) {
  GtkWidget *uriEntry = gtk_entry_new();
  gtk_widget_add_css_class(uriEntry, "uri-bar");
  gtk_entry_set_placeholder_text(GTK_ENTRY(uriEntry), "🔎 Search");

  gtk_widget_set_hexpand(uriEntry, TRUE);

  g_signal_connect(uriEntry, "activate", G_CALLBACK(onUriActive), state);

  GtkEventController *focusCtrl = gtk_event_controller_focus_new();
  g_signal_connect(focusCtrl, "enter", G_CALLBACK(onFocus), uriEntry);
  gtk_widget_add_controller(uriEntry, focusCtrl);

  UriBarData *d = g_new(UriBarData, 1);
  d->state = state;
  d->entry = uriEntry;
  g_signal_connect(state->webView, "notify::uri", G_CALLBACK(onSyncUri), d);

  return uriEntry;
}
