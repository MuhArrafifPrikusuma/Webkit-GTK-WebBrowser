#ifndef TABS_H
#define TABS_H

#include "../state.h"

char *tabCachePath(int id);
void saveToDisk(Tab *tab);
void loadFromDisk(Tab *tab);
void deleteTabFromDisk(int id);
GtkWidget *makeTabRow(AppState *state, int index);
void switchTab(AppState *state, int index);
void addNewTab(AppState *state, const char *uri);

#endif
