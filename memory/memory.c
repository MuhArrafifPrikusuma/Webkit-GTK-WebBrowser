#include "memory.h"
#include "../tabs/tabs.h"
#include "../webview/webview.h"
#include "gio/gio.h"
#include "glib-object.h"
#include "glib.h"
#include "glibconfig.h"
#include "gtk/gtk.h"
#include "webkit/WebKitSettings.h"
#include "webkit/WebKitUserContentManager.h"
#include <dirent.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>
#include <webkit/webkit.h>

// how often watchdog check memory
#define WATCHDOG_INTERVAL_MS 50000

#define HIGH_WATERMARK_KB (400 * 1024)

// read rss of a specific process id
static gulong getPidRssKb(pid_t pid) {
  char path[64];
  snprintf(path, sizeof(path), "/proc/%d/status", pid);
  FILE *f = fopen(path, "r");
  if (!f)
    return 0;

  char line[256];
  gulong rss = 0;
  while (fgets(line, sizeof(line), f)) {
    if (sscanf(line, "VmRSS: %lu kB", &rss) == 1)
      break;
  }
  fclose(f);
  return rss;
}

// read process id parent
static pid_t getPidParent(pid_t pid) {
  char path[64];
  snprintf(path, sizeof(path), "/proc/%d/status", pid);
  FILE *f = fopen(path, "r");
  if (!f)
    return 0;
  char line[256];
  pid_t ppid = 0;
  while (fgets(line, sizeof(line), f)) {
    if (sscanf(line, "PPid: %d", &ppid) == 1)
      break;
  }
  fclose(f);
  return ppid;
}

// Get The sum of all webkit process
gulong totalRssKb(void) {
  pid_t selfPid = getpid();
  gulong total = getPidRssKb(selfPid);

  DIR *proc = opendir("/proc");
  if (!proc)
    return total;

  struct dirent *entry;
  while ((entry = readdir(proc)) != NULL) {
    if (entry->d_type != DT_DIR)
      continue;
    char *end;
    pid_t pid = strtol(entry->d_name, &end, 10);
    if (*end != '\0' || pid == selfPid)
      continue;

    if (getPidParent(pid) == selfPid)
      total += getPidRssKb(pid);
  }
  closedir(proc);
  return total;
}

gulong getResidentMemoryKb(void) { return getPidRssKb(getpid()); }

// WARNING: Don't use this it might crash
// void enforeVirtMemoryCap() {
//   struct rlimit limit;
//   gulong max = 1500000000;
//
//   limit.rlim_cur = max;
//   limit.rlim_max = max;
//
//   if (setrlimit(RLIMIT_AS, &limit) != 0)
//     perror("failed to bind virtual memory to it's limit");
// }

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

static gboolean burnGarbage(gpointer userData) {
  AppState *state = userData;
  for (guint i = 0; i < state->tabs->len; i++) {
    Tab *tab = g_ptr_array_index(state->tabs, i);
    if (tab->garbage) {
      saveToDisk(tab);
      tab->garbage = FALSE;
    }
  }
  return G_SOURCE_CONTINUE;
}

// FIX: this crashes everything so fix this bullshit before actually using it
// void destroyOldViewer(GtkWidget *container, AppState *state) {
//  if (!state || state->webView)
//    return;
//
//  WebKitWebView *oldView = WEBKIT_WEB_VIEW(state->webView);
//
//  g_signal_handlers_disconnect_by_data(GTK_WIDGET(oldView), state);
//
//  WebKitSettings *settings = webkit_web_view_get_settings(oldView);
//  WebKitNetworkSession *network =
//  webkit_web_view_get_network_session(oldView); WebKitUserContentManager
//  *cManager =
//      webkit_web_view_get_user_content_manager(oldView);
//
//  const char *currentUri = webkit_web_view_get_uri(oldView);
//  char *savedUri = currentUri ? g_strdup(currentUri) : NULL;
//
//  WebKitWebView *newView = WEBKIT_WEB_VIEW(g_object_new(
//      WEBKIT_TYPE_WEB_VIEW, "settings", settings, "network-session", network,
//      "user-content-manager", cManager, NULL));
//
//  gtk_widget_set_hexpand(GTK_WIDGET(newView), TRUE);
//  gtk_widget_set_vexpand(GTK_WIDGET(newView), TRUE);
//
//  gtk_overlay_set_child(GTK_OVERLAY(container), GTK_WIDGET(newView));
//
//  state->webView = GTK_WIDGET(newView);
//
//  g_signal_connect(state->webView, "notify::uri", G_CALLBACK(onUriChange),
//                   state);
//  g_signal_connect(state->webView, "notify::title", G_CALLBACK(onTitleChange),
//                   state);
//  g_signal_connect(state->webView, "load-changed", G_CALLBACK(onLoadChange),
//                   state);
//
//  if (savedUri && g_strcmp0(savedUri, "about:blank") == 0)
//    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(state->webView), savedUri);
//
//  g_free(savedUri);
//
//  g_print("[memory] killed old webView and replaced it Successfully");
//}

static void onCacheCleared(GObject *source, GAsyncResult *rs,
                           gpointer userData) {
  WebKitWebsiteDataManager *manager = WEBKIT_WEBSITE_DATA_MANAGER(source);
  g_autoptr(GError) error = NULL;

  if (!webkit_website_data_manager_clear_finish(manager, rs, &error)) {
    g_warning("[memory] Failed to destroy cache: %s", error->message);
    return;
  }
  malloc_trim(0);
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

  webkit_website_data_manager_clear(manager, types, 0, NULL, onCacheCleared,
                                    NULL);
}

typedef struct {
  AppState *state;
  GtkWidget *overlay;
  gulong peakRssKb; // track peak memory
} WatchdogIsWatching;

static gboolean watchdogTick(gpointer userData) {
  WatchdogIsWatching *d = userData;
  gulong rssKb = getResidentMemoryKb();
  gulong totalRss = totalRssKb();

  if (totalRss > d->peakRssKb)
    d->peakRssKb = totalRss;

  g_print("[memory] watchdog -- self RSS: %.1f MB\ntotal RSS: %.1f MB\n peak: "
          "%.1lf MB\n",
          rssKb / 1024.0, totalRss / 1024.0, d->peakRssKb / 1024.0);

  if (totalRss > HIGH_WATERMARK_KB) {
    g_print("[memory] high watermark exceeded (%.1f MB) -- WatchDog GO KILL "
            "EMMM!!\n",
            totalRss / 1024.0);
    reclaimMemory(d->state);
    //    destroyOldViewer(d->overlay, d->state);
  }
  return G_SOURCE_CONTINUE;
}

void startMemoryWatchdog(AppState *state) {
  WatchdogIsWatching *d = g_new0(WatchdogIsWatching, 1);
  d->state = state;
  d->peakRssKb = 0;

  g_timeout_add(WATCHDOG_INTERVAL_MS, watchdogTick, d);
  g_timeout_add(5000, burnGarbage, state);

  g_print("[memory] watchdog hunting - interval %ds, watermark %dMB\n",
          WATCHDOG_INTERVAL_MS / 1000, HIGH_WATERMARK_KB / 1024);
}
