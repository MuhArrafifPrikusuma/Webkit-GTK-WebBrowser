#include "css/css.h"
#include "gdk/gdk.h"
#include "glib-object.h"
#include "glib.h"
#include "memory/memory.h"
#include "sidebar/sidebar.h"
#include "spotlight/spotlight.h"
#include "state.h"
#include "tabs/tabs.h"
#include "toolbar/toolbar.h"
#include "webview/webview.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <webkit/webkit.h>

static gboolean onWindowKey(GtkEventControllerKey *key, guint keyvalue,
                            guint keycode, GdkModifierType modifiers,
                            gpointer userData) {
  AppState *state = userData;
  gboolean ctrlHeld = (modifiers & GDK_CONTROL_MASK) != 0;

  if (ctrlHeld && (keyvalue == GDK_KEY_l || keyvalue == GDK_KEY_k)) {
    if (gtk_widget_get_visible(state->spotlight)) {
      hideSpotlight(state->spotlight);
    } else {
      showSpotlight(state->spotlight);
    }
    return TRUE;
  }
  return FALSE;
}

// build the ui
static void activate(GtkApplication *app, gpointer userData) {
  loadCSS();

  // create a window
  GtkWidget *window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "WebView");
  gtk_window_set_default_size(GTK_WINDOW(window), 1200, 700);

  // sets appstate for the very first tab
  AppState *state = g_new0(AppState, 1);
  state->tabs = g_ptr_array_new();
  state->active = 0;
  state->nextTabId = 0;

  // create a directory
  g_mkdir_with_parents(TAB_CACHE_DIR, 0700);

  // clear leftover from previous session
  GDir *cacheDir = g_dir_open(TAB_CACHE_DIR, 0, NULL);
  if (cacheDir) {
    const char *fName;
    while ((fName = g_dir_read_name(cacheDir)) != NULL) {
      char *full = g_strdup_printf("%s/%s", TAB_CACHE_DIR, fName);
      remove(full);
      g_free(full);
    }
    g_dir_close(cacheDir);
  }

  // create sidebar widget
  GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_set_size_request(sidebar, 190, -1);
  gtk_widget_set_hexpand(sidebar, FALSE);
  gtk_widget_add_css_class(sidebar, "sidebar");
  gtk_widget_set_margin_bottom(sidebar, 0);
  gtk_widget_set_margin_top(sidebar, 0);

  GtkWidget *toolbar = makeToolbar(state);
  state->searchBar = g_object_get_data(G_OBJECT(toolbar), "search-bar");
  gtk_box_append(GTK_BOX(sidebar), toolbar);

  GtkWidget *newTabBtn = gtk_button_new_with_label("+ New Tab");
  gtk_button_set_has_frame(GTK_BUTTON(newTabBtn), FALSE);
  gtk_widget_add_css_class(newTabBtn, "new-tab-btn");
  gtk_widget_set_margin_start(newTabBtn, 8);
  gtk_widget_set_margin_end(newTabBtn, 8);
  gtk_widget_set_halign(newTabBtn, GTK_ALIGN_FILL);
  gtk_box_append(GTK_BOX(sidebar), newTabBtn);

  // append tabList to sidebar
  state->tabList = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  gtk_box_append(GTK_BOX(sidebar), state->tabList);

  // pass tabList data to magnifier and magnify those tabs based on magnifier
  // motion calculation
  GtkEventController *magCtrl = gtk_event_controller_motion_new();
  Magnifier *magData = makeMagData(state, state->tabList);

  g_signal_connect(magCtrl, "motion", G_CALLBACK(onMagMotion), magData);
  g_signal_connect(magCtrl, "leave", G_CALLBACK(onMagLeave), magData);
  gtk_widget_add_controller(state->tabList, magCtrl);

  // keeps uri and title current and restore scroll position
  state->webView = webkit_web_view_new();
  GdkRGBA transparentBG = {0.25, 0.26, 0.36, 0.5};
  webkit_web_view_set_background_color(WEBKIT_WEB_VIEW(state->webView),
                                       &transparentBG);
  gtk_widget_add_css_class(GTK_WIDGET(state->webView), "web-view");

  configureWebkit(state);

  g_signal_connect(state->webView, "notify::uri", G_CALLBACK(onUriChange),
                   state);
  g_signal_connect(state->webView, "notify::title", G_CALLBACK(onTitleChange),
                   state);
  g_signal_connect(state->webView, "load-changed", G_CALLBACK(onLoadChange),
                   state);

  // reveal sidebar
  GtkWidget *revealer = gtk_revealer_new();
  gtk_revealer_set_transition_type(GTK_REVEALER(revealer),
                                   GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT);
  gtk_revealer_set_transition_duration(GTK_REVEALER(revealer), 200);

  gtk_revealer_set_child(GTK_REVEALER(revealer), sidebar);
  gtk_revealer_set_reveal_child(GTK_REVEALER(revealer), FALSE);
  gtk_widget_set_valign(revealer, GTK_ALIGN_FILL);
  gtk_widget_set_halign(revealer, GTK_ALIGN_START);

  state->spotlight = makeSpotlight(state);

  // set webView at the very bottom of the overlay and revealer on top of it
  GtkWidget *overlay = gtk_overlay_new();
  gtk_overlay_set_child(GTK_OVERLAY(overlay), state->webView);
  gtk_overlay_add_overlay(GTK_OVERLAY(overlay), revealer);
  gtk_overlay_add_overlay(GTK_OVERLAY(overlay), state->spotlight);
  // don't change webView size
  gtk_overlay_set_measure_overlay(GTK_OVERLAY(overlay), revealer, FALSE);
  gtk_overlay_set_measure_overlay(GTK_OVERLAY(overlay), state->spotlight,
                                  FALSE);
  // click pass throught revealer and will only register to revealer if it's
  // visible
  gtk_overlay_set_clip_overlay(GTK_OVERLAY(overlay), revealer, TRUE);
  gtk_overlay_set_clip_overlay(GTK_OVERLAY(overlay), state->spotlight, FALSE);

  // captures mouse movement in overlay
  GtkEventController *motionCtrl = gtk_event_controller_motion_new();
  g_signal_connect(motionCtrl, "motion", G_CALLBACK(onMouseMotion), revealer);
  g_signal_connect(motionCtrl, "leave", G_CALLBACK(onMouseLeave), revealer);
  gtk_widget_add_controller(overlay, motionCtrl);

  GtkEventController *keyCtrl = gtk_event_controller_key_new();
  g_signal_connect(keyCtrl, "key-pressed", G_CALLBACK(onWindowKey), state);
  gtk_widget_add_controller(window, keyCtrl);

  // create a new tab on button click
  g_signal_connect(newTabBtn, "clicked", G_CALLBACK(onNewTab), state);
  // remove when spotlight is implemented
  addNewTab(state, "about:blank");

  startMemoryWatchdog(state, overlay);

  // set overlay as window content and make the window visible
  gtk_window_set_child(GTK_WINDOW(window), overlay);
  gtk_window_present(GTK_WINDOW(window));
}
int main(int argc, char **argv) {
  GtkApplication *app;
  int status;
  // init
  app = gtk_application_new("org.webview.app", G_APPLICATION_NON_UNIQUE);
  // register activate to run when app starts
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
  status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}
