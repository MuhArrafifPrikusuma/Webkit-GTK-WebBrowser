#include "memory.h"
#include "gio/gio.h"
#include "glib-object.h"
#include "glib.h"
#include "glibconfig.h"
#include "gtk/gtk.h"
#include "webkit/WebKitSettings.h"
#include "webkit/WebKitUserContentManager.h"
#include <stdio.h>
#include <webkit/webkit.h>

// how often watchdog check memory
#define WATCHDOG_INTERVAL_MS 50000

#define HIGH_WATERMARK_KB (400 * 1024)

// manage virtual memory resident
gulong getResidentMemoryKb(void) {
  FILE *f = fopen("/proc/self/status", "r");
  if (!f)
    return 0;

  char line[256];
  // virtual memory resident set size
  gulong vmrss = 0;
  while (fgets(line, sizeof(line), f)) {
    // string scan formatted
    if (sscanf(line, "VmRSS: %lu kB", &vmrss))
      break;
  }
  fclose(f);
  return vmrss;
}

void configureWebkit(AppState *state) {
  WebKitWebContext *ctx = webkit_web_context_get_default();

  webkit_web_context_set_cache_model(ctx, WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER);

  WebKitSettings *settings =
      webkit_web_view_get_settings(WEBKIT_WEB_VIEW(state->webView));

  webkit_settings_set_enable_page_cache(settings, FALSE);

  webkit_settings_set_hardware_acceleration_policy(
      settings, WEBKIT_HARDWARE_ACCELERATION_POLICY_ALWAYS);

  webkit_settings_set_enable_media_stream(settings, FALSE);

  webkit_settings_set_enable_write_console_messages_to_stdout(settings, FALSE);

  g_print("[memory] Webkit configured:  document-browser cache, and hardware "
          "accel, and disabled write to stdout and mediastream\n");
}

// FIX: this crashes everything so fix this bullshit before actually using it
void destroyOldViewer(GtkBox *container, WebKitWebView *oldView,
                      AppState *state) {
  if (!oldView)
    return;

  WebKitSettings *settings = webkit_web_view_get_settings(oldView);
  WebKitNetworkSession *network = webkit_web_view_get_network_session(oldView);
  WebKitUserContentManager *cManager =
      webkit_web_view_get_user_content_manager(oldView);

  WebKitWebView *newView = WEBKIT_WEB_VIEW(g_object_new(
      WEBKIT_TYPE_WEB_VIEW, "settings", settings, "network-session", network,
      "user-content-manager", cManager, NULL));

  gtk_widget_set_hexpand(GTK_WIDGET(newView), TRUE);
  gtk_widget_set_vexpand(GTK_WIDGET(newView), TRUE);

  state->webView = GTK_WIDGET(newView);

  gtk_box_remove(container, GTK_WIDGET(oldView));
  gtk_box_append(container, GTK_WIDGET(newView));

  g_print("[memory] Created newView Successfully");
}

static void onCacheCleared(GObject *source, GAsyncResult *rs,
                           gpointer userData) {
  WebKitWebsiteDataManager *manager = WEBKIT_WEBSITE_DATA_MANAGER(source);
  g_autoptr(GError) error = NULL;

  if (!webkit_website_data_manager_clear_finish(manager, rs, &error)) {
    g_warning("[memory] Failed to destroy cache: %s", error->message);
    return;
  }
  g_print("[memory] Webkit cache cleared -- RSS now %.1f MB\n",
          getResidentMemoryKb() / 1024.0);
}

void reclaimMemory(AppState *state) {
  WebKitWebContext *ctx = webkit_web_context_get_default();

  if (!state || !state->webView)
    return;

  WebKitNetworkSession *session =
      webkit_web_view_get_network_session(WEBKIT_WEB_VIEW(state->webView));
  if (!session)
    return;
  WebKitWebsiteDataManager *manager =
      webkit_network_session_get_website_data_manager(session);
  if (!WEBKIT_IS_WEBSITE_DATA_MANAGER(manager))
    return;

  WebKitWebsiteDataTypes types =
      WEBKIT_WEBSITE_DATA_MEMORY_CACHE | WEBKIT_WEBSITE_DATA_DISK_CACHE;

  webkit_website_data_manager_clear(manager, types, 0, NULL, NULL, NULL);
}

typedef struct {
  AppState *state;
  GtkBox *container;
  gulong peakRssKb; // track peak memory
} WatchdogIsWatching;

static gboolean watchdogTick(gpointer userData) {
  WatchdogIsWatching *d = userData;
  gulong rssKb = getResidentMemoryKb();

  if (rssKb > d->peakRssKb)
    d->peakRssKb = rssKb;

  g_print("[memory] watchdog -- RSS: %.1f MB peak: %.1lf MB\n", rssKb / 1024.0,
          d->peakRssKb / 1024.0);

  if (rssKb > HIGH_WATERMARK_KB) {
    g_print("[memory] high watermark exceeded (%.1f MB) -- WatchDog GO KILL "
            "EMMM!!\n",
            rssKb / 1024.0);
    reclaimMemory(d->state);
    destroyOldViewer(d->container, WEBKIT_WEB_VIEW(d->state->webView),
                     d->state);
  }
  return G_SOURCE_CONTINUE;
}

void startMemoryWatchdog(AppState *state) {
  WatchdogIsWatching *d = g_new0(WatchdogIsWatching, 1);
  d->state = state;
  d->peakRssKb = 0;

  g_timeout_add(WATCHDOG_INTERVAL_MS, watchdogTick, d);

  g_print("[memory] watchdog hunting - interval %ds, watermark %dMB\n",
          WATCHDOG_INTERVAL_MS / 1000, HIGH_WATERMARK_KB / 1024);
}
