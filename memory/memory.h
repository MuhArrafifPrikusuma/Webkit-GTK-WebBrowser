#ifndef MEMORY_H
#define MEMORY_H

#include "../state.h"
#include <gtk/gtk.h>
#include <time.h>

void configureWebkit(AppState *state);

void reclaimMemory(AppState *state);

void startMemoryWatchdog(AppState *state);

gulong getResidentMemoryKb(void);

#endif // !MEMORY_H
