#ifndef MEMORY_H
#define MEMORY_H

#include "../state.h"
#include "gtk/gtkshortcut.h"
#include <gtk/gtk.h>
#include <time.h>

void configureWebkit(AppState *state);

void reclaimMemory(AppState *state);

void startMemoryWatchdog(AppState *state, GtkWidget *overlay);

gulong getResidentMemoryKb(void);

void destroyOldViewer(GtkWidget *overlay, AppState *state);

#endif // !MEMORY_H
