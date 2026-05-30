#include "webview.h"
#include "../tabs/tabs.h"
#include <gtk/gtk.h>
#include <webkit/webkit.h>

// releases the old uri and copies the new one
void onUriChange(WebKitWebView *wv, GParamSpec *ps, gpointer userData) {
  AppState *state = userData;
  const char *uri = webkit_web_view_get_uri(wv);

  if (!uri || g_strcmp0(uri, "about:blank") == 0)
    return;

  Tab *tab = g_ptr_array_index(state->tabs, state->active);

  g_free(tab->uri);
  tab->uri = g_strdup(uri);
}

// replaces the old title with a new one
void onTitleChange(WebKitWebView *wv, GParamSpec *ps, gpointer userData) {
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

void onLoadChange(WebKitWebView *wv, WebKitLoadEvent event, gpointer userData) {
  AppState *state = userData;
  // don't return anything before page finish loading
  if (event != WEBKIT_LOAD_FINISHED)
    return;

  Tab *tab = g_ptr_array_index(state->tabs, state->active);
  if (tab->scrollY <= 0)
    return;

  // build javascript string to dynamically with the saved scrollY value on that
  // tab
  char *js = g_strdup_printf("window.scrollTo(0, %.2f);", tab->scrollY);
  webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(wv), js, -1, NULL, NULL,
                                      NULL, NULL, NULL);
  g_free(js);
}
