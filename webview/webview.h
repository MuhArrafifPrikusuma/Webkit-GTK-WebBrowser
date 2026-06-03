#ifndef WEBVIEW_H
#define WEBVIEW_H

#include "../state.h"
#include <webkit/webkit.h>

void onUriChange(WebKitWebView *wv, GParamSpec *ps, gpointer userData);
void onTitleChange(WebKitWebView *wv, GParamSpec *ps, gpointer userData);
void onLoadChange(WebKitWebView *wv, WebKitLoadEvent event, gpointer userData);

#endif
