#ifndef SIDEBAR_H
#define SIDEBAR_H

#include "../state.h"
#include <gtk/gtk.h>

void onMouseMotion(GtkEventControllerMotion *motion, double x, double y,
                   gpointer userData);
void onMouseLeave(GtkEventControllerMotion *motion, gpointer userData);
void onNewTab(GtkButton *btn, gpointer userData);

#endif
