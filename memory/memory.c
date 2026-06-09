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

#define WATCHDOG_INTERVAL_MS 50000
#define HIGH_WATERMARK_KB (400 * 1024)
#define WATCHDOG_COOLDOWN_MS 120000

static void getPidStats(pid_t pid, pid_t *ppid, gulong *rss_pages) {
  char path[64];
  snprintf(path, sizeof(path), "/proc/%d/stat", pid);
  FILE *f = fopen(path, "r");
  if (!f) {
    if (ppid)
      *ppid = 0;
    if (rss_pages)
      *rss_pages = 0;
    return;
  }

  long unsigned int tmp_rss = 0;
  int tmp_ppid = 0;

  // Field 4 is PPID. Then fast-forward read to field 24 (RSS position)
  if (fscanf(f, "%*d %*s %*c %d", &tmp_ppid) == 1) {
    if (ppid)
      *ppid = tmp_ppid;

    for (int i = 5; i <= 24; i++) {
      if (fscanf(f, "%lu", &tmp_rss) != 1)
        break;
    }
    if (rss_pages)
      *rss_pages = tmp_rss;
  }

  fclose(f);
}

static gulong getPidRssKb(pid_t pid) {
  gulong rss_pages = 0;
  getPidStats(pid, NULL, &rss_pages);
  return rss_pages * (sysconf(_SC_PAGESIZE) / 1024);
}

gulong totalRssKb(void) {
  pid_t selfPid = getpid();
  gulong total = getPidRssKb(selfPid);

  DIR *proc = opendir("/proc");
  if (!proc)
    return total;

  struct dirent *entry;
  long page_size_kb = sysconf(_SC_PAGESIZE) / 1024;

  while ((entry = readdir(proc)) != NULL) {
    if (entry->d_type != DT_DIR)
      continue;

    char *end;
    pid_t pid = strtol(entry->d_name, &end, 10);
    if (*end != '\0' || pid == selfPid)
      continue;

    pid_t ppid = 0;
    gulong rss_pages = 0;
    getPidStats(pid, &ppid, &rss_pages);

    // If this process belongs to our WebKit UI process container
    if (ppid == selfPid) {
      total += (rss_pages * page_size_kb);
    }
  }
  closedir(proc);
  return total;
}

gulong getResidentMemoryKb(void) { return getPidRssKb(getpid()); }

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

  g_print("[memory] Webkit configured: document-viewer, forced HW accel.\n");
}

static gboolean burnGarbage(gpointer userData) {
  AppState *state = userData;
  if (!state || !state->tabs)
    return G_SOURCE_CONTINUE;

  for (guint i = 0; i < state->tabs->len; i++) {
    Tab *tab = g_ptr_array_index(state->tabs, i);
    if (tab && tab->garbage) {
      saveToDisk(tab);
      tab->garbage = FALSE;
    }
  }
  return G_SOURCE_CONTINUE;
}

static gboolean doMallocTrim(gpointer data) {
  malloc_trim(0);
  g_print("[memory] Async malloc_trim complete -- Current RSS: %.1f MB\n",
          getResidentMemoryKb() / 1024.0);
  return G_SOURCE_REMOVE;
}

static void onCacheCleared(GObject *source, GAsyncResult *rs,
                           gpointer userData) {
  WebKitWebsiteDataManager *manager = WEBKIT_WEBSITE_DATA_MANAGER(source);
  g_autoptr(GError) error = NULL;

  if (!webkit_website_data_manager_clear_finish(manager, rs, &error)) {
    g_warning("[memory] Failed to destroy cache: %s", error->message);
    return;
  }

  g_print("[memory] Webkit cache cleared out safely.\n");
  g_idle_add(doMallocTrim, NULL);
}

static gboolean safeToReclaim(AppState *state) {
  if (!state || !state->webView)
    return FALSE;

  double progress = webkit_web_view_get_estimated_load_progress(
      WEBKIT_WEB_VIEW(state->webView));
  return progress >= 1.0;
}

void reclaimMemory(AppState *state) {
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
  gint64 lastReclaim;
  gulong peakRssKb;
} WatchdogIsWatching;

static gboolean watchdogTick(gpointer userData) {
  WatchdogIsWatching *d = userData;
  gulong rssKb = getResidentMemoryKb();
  gulong totalRss = totalRssKb();

  if (totalRss > d->peakRssKb)
    d->peakRssKb = totalRss;

  g_print("[memory] watchdog -- self RSS: %.1f MB | total RSS: %.1f MB | peak: "
          "%.1f MB\n",
          rssKb / 1024.0, totalRss / 1024.0, d->peakRssKb / 1024.0);

  // If we're under the limit, everything is fine. Early exit.
  if (totalRss <= HIGH_WATERMARK_KB) {
    return G_SOURCE_CONTINUE;
  }

  gint64 now = g_get_monotonic_time();
  gint64 elapsed = now - d->lastReclaim;
  gint64 cooldown = (gint64)WATCHDOG_COOLDOWN_MS * 1000;

  if (elapsed < cooldown) {
    g_print("[memory] Memory limit exceeded, but watchdog is cooling down. "
            "%.0fs remaining\n",
            (double)(cooldown - elapsed) / 1e6);
    return G_SOURCE_CONTINUE;
  }

  if (!safeToReclaim(d->state)) {
    g_print("[memory] Watermark exceeded, but skipped because page is actively "
            "loading\n");
    return G_SOURCE_CONTINUE;
  }

  // Clear and clean up memory safely
  g_print("[memory] High watermark reached (%.1f MB) -- Reclaiming caches...\n",
          totalRss / 1024.0);
  reclaimMemory(d->state);

  d->lastReclaim = now;

  return G_SOURCE_CONTINUE;
}

void startMemoryWatchdog(AppState *state) {
  WatchdogIsWatching *d = g_new0(WatchdogIsWatching, 1);
  d->state = state;
  d->peakRssKb = 0;
  d->lastReclaim = 0;

  g_timeout_add(WATCHDOG_INTERVAL_MS, watchdogTick, d);
  g_timeout_add(5000, burnGarbage, state);

  g_print(
      "[memory] Watchdog active - Interval: %ds, Limit: %dMB, Cooldown: %ds\n",
      WATCHDOG_INTERVAL_MS / 1000, HIGH_WATERMARK_KB / 1024,
      WATCHDOG_COOLDOWN_MS / 1000);
}
