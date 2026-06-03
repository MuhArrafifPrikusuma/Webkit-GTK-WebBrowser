#ifndef SPOTLIGHT_H
#define SPOTLIGHT_H

#include "../state.h"
#include "gtk/gtkshortcut.h"
#include <gtk/gtk.h>

void hideSpotlight(GtkWidget *spotlight);

void showSpotlight(GtkWidget *spotlight);

GtkWidget *makeSpotlight(AppState *state);

#endif // !SPOTLIGHT_H
