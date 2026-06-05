## main.c
  
  mainly handles launching app and build ui

### Functions
  
    static gboolean onWindowKey(GtkEventControllerKey *key, guint keyvalue, 
                                guint keycode, GdkModifierType modifiers, 
                                gpointer userData);
  
  Handles keyboard shortcut for spotlight search (will soon be moved to dedicated shortcuts files)
  
    static void activate(GtkApplication *app, gpointer userData);
  
  - window init
  - create the first tab
  - create ui elements like sidebar, toolbar, revealer, webView rendering, tabs, overlay and also detect changes and apply changes and initiate memory watchdog and webkit settings

    int main(int argc, char **argv) 
  
  What it does: set app domain and initiate the start of the browser and actively maintain the active function to keep running until the app is closed
  
## tabs.c

  handles tabs creation, deletion and tab animation 

### Functions

#### Tab action logic

    char *tabCachePath(int id);

  set up file path string for the cache

    void saveToDisk(Tab *tab);

  create a tmp file and then write uri, title and scroll position in this order 
  uri
  title
  scroll Position
  and then change the file name to tab $id;

    void loadFromDisk(Tab *tab);

  Copy data from disk to ram

    void deleteTabFromDisk(int id);

  Delete cache file

    void closeTab(AppState *state, int index);


  Close tab, remove it's data from disk and ram and determine the new index lenght and
  active tab

#### Magnifier Effect

    static int findTableIndexById(AppState *state, int id);

  look for tab index position by looping through index and look for tab->id in every index
  until it found one that matches the given id that it searches

    typedef struct {
      AppState *state;
      GtkWidget *tabList;
    } Magnifier;

  bundle Appstate and tabList for Magnifier functions
  
    Magnifier *makeMagData(AppState *state, GtkWidget *tabList);
  
  allocate memory and initiate Magnifier data context

    void onMagMotion(GtkEventControllerMotion *motion, double x, double y, gpointer userData);

  basically detect the distance between cursor and tabList and then stretch it depending
  on how far or how close it is

    void onMagLeave(GtkEventControllerMotion *motion, gpointer userData);

  detect if it leave to stop magnifier effect

#### Other Animations

    static void onRowEnter(GtkEventControllerMotion *motion, double x, double y, gpointer userData);

  Detect mouse position and check if it's within 30 pixels at the very end of the overlay
  it will reveal the close tab button

    static void onRowLeave(GtkEventControllerMotion *motion, gpointer userData);

  if cursor leave it it's gone, is that simple

#### Tab Actions

    static void onTabClick(GtkGestureClick *gesture, int nPress, double x, double y, gpointer userData);

 detect if the tab if tabRow is clicked using the keystring "app-state" and then use 
 findTableIndexById to see the current index of the clicked tab and switch to it

    void switchTab(AppState *state, int index);
  
  pass state and new tab id and start evaluation js to find user scroll position

    static void afterSaveSwitch(GObject *wv, GAsyncResult *result, gpointer userData);

  extract scroll data, move active-tab css class to the new active tab and load tab data from disk

#### Widgets

    GtkWidget *makeTabRow(AppState *state, int index);

  create the alot of widgets (row, overlay , rowRevealer, revealer for close btn and close button)

    void addNewTab(AppState *state, const char *uri);

  addNewTab and immediately switch to it
