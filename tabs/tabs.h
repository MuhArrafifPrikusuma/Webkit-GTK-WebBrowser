#ifndef TABS_H
#define TABS_H

#include "../memory/memory.h"
#include "../state.h"
#include "glib.h"
#include "gtk/gtk.h"
#include "gtk/gtkshortcut.h"

char *tabCachePath(int id);
void loadFromDisk(Tab *tab);
void deleteTabFromDisk(int id);
GtkWidget *makeTabRow(AppState *state, int index);
void switchTab(AppState *state, int index);
void addNewTab(AppState *state, const char *uri);
void closeTab(AppState *state, int index);

typedef struct {
  AppState *state;
  GtkWidget *tabList;
} Magnifier;

Magnifier *makeMagData(AppState *state, GtkWidget *tabList);
void onMagMotion(GtkEventControllerMotion *motion, double x, double y,
                 gpointer userData);
void onMagLeave(GtkEventControllerMotion *motion, gpointer userData);

void saveToDisk(Tab *tab);

#endif
