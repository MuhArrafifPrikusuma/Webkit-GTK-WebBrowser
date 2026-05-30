#ifndef STATE_H
#define STATE_H

#include <gtk/gtk.h>
#include <webkit/webkit.h>

#define TAB_CACHE_DIR "/tmp/browser_tabs"

/*
 * scrollY: store scroll position
 * webView: Tracks tab's URI
 * tabRow: contain label and favicon
 * tabLabel: pointer to GtkLabel inside row
 * TODO: display the actual favicon instead of placeholder
 * favicon: pointer to GtkImage to show favicon
 */
typedef struct {
  char *uri;
  char *title;
  double scrollY;
  GtkWidget *tabRow;
  GtkLabel *tabLabel;
  GtkImage *favicon;
  int id;
} Tab;

/*
 * tabList: Hold all tabs rows
 * webView: render websites
 * tabs: Glib dynamic arrays to store tabs which start from pointer at index 0
 * active: integer index of currently selected tab
 * nextTabId; increment id
 */
typedef struct {
  GtkWidget *tabList;
  GtkWidget *webView;
  GPtrArray *tabs;
  int active;
  int nextTabId;
} AppState;

#endif
