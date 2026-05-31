#ifndef SEARCH_H
#define SEARCH_H

#include "../state.h"

typedef struct {
  AppState *state;
  GtkWidget *entry;
} UriBarData;

GtkWidget *makeUriSearch(AppState *state);
void syncSearch(UriBarData *d, const char *uri);

#endif // !SEARCH_H
