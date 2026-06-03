#include "spotlight.h"
#include "gdk/gdk.h"
#include "gdk/gdkkeysyms.h"
#include "glib-object.h"
#include "glib.h"
#include "glibconfig.h"
#include "gtk/gtk.h"
#include "gtk/gtkshortcut.h"
#include <iso646.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static gboolean isUri(const char *text) {
  if (strstr(text, "://"))
    return TRUE;
  if (g_str_has_prefix(text, "localhost"))
    return TRUE;
  if (strchr(text, '.') && !strchr(text, ' '))
    return TRUE;
  return FALSE;
}

static void navigateFromSpotlight(AppState *state, const char *text,
                                  GtkWidget *spotlight) {
  if (!text || *text == '\0')
    return;

  char *uri;
  if (isUri(text)) {
    uri = strstr(text, "://") ? g_strdup(text)
                              : g_strdup_printf("https://%s", text);
  } else if (g_strcmp0(text, "about:blank") == 0) {
    uri = g_strdup(text);
  } else {
    char *encoded = g_uri_escape_string(text, NULL, FALSE);
    uri = g_strdup_printf("https://search.google.com/search?q=%s", encoded);
    g_free(encoded);
  }
  webkit_web_view_load_uri(WEBKIT_WEB_VIEW(state->webView), uri);
  g_free(uri);

  hideSpotlight(spotlight);
}

typedef struct {
  AppState *state;
  GtkWidget *spotlight;
} SpotlightData;

static void onActivate(GtkEntry *entry, gpointer userData) {
  SpotlightData *d = userData;
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));

  navigateFromSpotlight(d->state, text, d->spotlight);

  gtk_editable_set_text(GTK_EDITABLE(entry), "");
}

static gboolean onSpotlightKey(GtkEventControllerKey *key, guint keyvalue,
                               guint keycode, GdkModifierType state,
                               gpointer userData) {

  if (keyvalue == GDK_KEY_Escape) {
    GtkWidget *spotlight = GTK_WIDGET(userData);
    hideSpotlight(spotlight);

    GtkWidget *entry = g_object_get_data(G_OBJECT(spotlight), "entry");
    if (entry)
      gtk_editable_set_text(GTK_EDITABLE(entry), "");
    return TRUE;
  }
  return FALSE;
}

static void onNotCardClick(GtkGestureClick *gesture, int nPress, double x,
                           double y, gpointer userData) {
  GtkWidget *spotlight = GTK_WIDGET(userData);
  hideSpotlight(spotlight);
  GtkWidget *entry = g_object_get_data(G_OBJECT(spotlight), "entry");
  if (entry)
    gtk_editable_set_text(GTK_EDITABLE(entry), "");
}

static void onCardClick(GtkGestureClick *gesture, int nPress, double x,
                        double y, gpointer userData) {
  gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

GtkWidget *makeSpotlight(AppState *state) {
  GtkWidget *notCard = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_add_css_class(notCard, "spotlight-background");
  gtk_widget_set_hexpand(notCard, TRUE);
  gtk_widget_set_vexpand(notCard, TRUE);

  gtk_widget_set_visible(notCard, FALSE);

  GtkWidget *spacerTop = gtk_label_new(NULL);
  gtk_widget_set_vexpand(spacerTop, TRUE);
  gtk_widget_set_hexpand(spacerTop, TRUE);
  gtk_box_append(GTK_BOX(notCard), spacerTop);

  GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_add_css_class(card, "spotlight-card");
  gtk_widget_set_halign(card, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(card, GTK_ALIGN_CENTER);
  gtk_widget_set_vexpand(card, FALSE);

  gtk_box_append(GTK_BOX(notCard), card);

  GtkWidget *spacerBottom = gtk_label_new(NULL);
  gtk_widget_set_vexpand(spacerBottom, TRUE);
  gtk_widget_set_hexpand(spacerBottom, FALSE);
  gtk_box_append(GTK_BOX(notCard), spacerBottom);

  GtkWidget *searchBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

  GtkWidget *icon = gtk_image_new_from_icon_name("system-search-symbolic");
  gtk_image_set_pixel_size(GTK_IMAGE(icon), 20);
  gtk_widget_add_css_class(icon, "spotlight-icon");
  gtk_box_append(GTK_BOX(searchBox), icon);

  GtkWidget *entry = gtk_entry_new();
  gtk_widget_set_name(entry, "spotlightentry");
  gtk_widget_add_css_class(entry, "spotlight-entry");
  gtk_entry_set_placeholder_text(GTK_ENTRY(entry),
                                 "Search or enter address...");
  gtk_widget_set_hexpand(entry, TRUE);

  gtk_entry_set_has_frame(GTK_ENTRY(entry), FALSE);

  gtk_widget_set_name(entry, "spotlight-entry");

  gtk_box_append(GTK_BOX(searchBox), entry);
  gtk_box_append(GTK_BOX(card), searchBox);

  g_object_set_data(G_OBJECT(notCard), "entry", entry);
  g_object_set_data(G_OBJECT(notCard), "state", state);

  GtkEventController *keyCtrl = gtk_event_controller_key_new();
  g_signal_connect(keyCtrl, "key-pressed", G_CALLBACK(onSpotlightKey), notCard);
  gtk_widget_add_controller(entry, keyCtrl);

  SpotlightData *sd = g_new(SpotlightData, 1);
  sd->state = state;
  sd->spotlight = notCard;
  g_signal_connect_data(entry, "activate", G_CALLBACK(onActivate), sd,
                        (GClosureNotify)g_free, G_CONNECT_DEFAULT);

  GtkGesture *notCardClick = gtk_gesture_click_new();
  g_signal_connect(notCardClick, "pressed", G_CALLBACK(onNotCardClick),
                   notCard);
  gtk_widget_add_controller(notCard, GTK_EVENT_CONTROLLER(notCardClick));

  GtkGesture *cardClick = gtk_gesture_click_new();
  g_signal_connect(cardClick, "pressed", G_CALLBACK(onCardClick), NULL);
  gtk_widget_add_controller(card, GTK_EVENT_CONTROLLER(cardClick));

  return notCard;
}

void showSpotlight(GtkWidget *spotlight) {
  AppState *state = g_object_get_data(G_OBJECT(spotlight), "state");
  if (state && state->webView) {
    webkit_web_view_evaluate_javascript(
        WEBKIT_WEB_VIEW(state->webView),
        "document.documentElement.style.transition = 'filter 0.18s ease';"
        "document.documentElement.style.filter = 'blur(6px) brightness(0.7)';",
        -1, NULL, NULL, NULL, NULL, NULL);
  }
  gtk_widget_set_visible(spotlight, true);
  GtkWidget *entry = g_object_get_data(G_OBJECT(spotlight), "entry");
  if (entry)
    gtk_widget_grab_focus(entry);
}

void hideSpotlight(GtkWidget *spotlight) {
  AppState *state = g_object_get_data(G_OBJECT(spotlight), "state");
  if (state && state->webView) {
    webkit_web_view_evaluate_javascript(
        WEBKIT_WEB_VIEW(state->webView),
        "document.documentElement.style.filter = '';"
        "document.documentElement.style.transition = 'filter 0.18s ease';",
        -1, NULL, NULL, NULL, NULL, NULL);
  }
  gtk_widget_set_visible(spotlight, FALSE);
}
