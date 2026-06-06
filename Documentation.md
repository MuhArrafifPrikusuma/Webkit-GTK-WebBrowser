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

## webkit.c

  this handle webkit rendering and properties like tab uri, titles, and scroll position

### Functions

    void onUriChange(WebKitWebView *wv, GParamSpec *ps, gpointer userData);

  changes the uri everytime the webview navigates anywhere

    void onTitleChange(WebKitWebView *wv, GParamSpec *ps, gpointer userData);

  changed the title and set a new label everytime webview navigates anywhere

    void onLoadChange(WebKitWebView *wv, WebKitLoadEvent event, gpointer userData);

  load scroll position

## sidebar.c

  detect mouse movements to collapse or reveal siderbar

### Functions

    void onMouseMotion(GtkEventControllerMotion *motion, double x, double y, gpointer userData);

  if mouse hovering on 20px at the very left of the browser it reveal the sidebar and will close if cursor is 10px outside the sidebar

    void onMouseLeave(GtkEventControllerMotion *motion, gpointer userData);

  if mouse is not inside those 20px or inside sidebar, the sidebar will collapse

    void onNewTab(GtkButton *btn, gpointer userData);

  just a wrapper for addNewTab();

## toolbar.c

  handle tools on the toolbar like back, forward, refresh, search, and later we will implement settings and maybe adblock? maybe even js toggle if im not lazy

### Functions

    static void onBack(GtkButton *btn, gpointer userData);

  navigate to previous page, if there is no previous page then do nothing

    static void onForward(GtkButton *btn, gpointer userData);

  navigate to the next page

    static void onRefresh(GtkButton *btn, gpointer userData);

  refresh page

    GtkWidget *makeToolbar(AppState *state);

  make tool bar widget with back, forward, refresh, search and soon settings

## search.c

  handle search widget and logic (merge the logic with spotlight search searching logic later);

### Functions

    static gboolean isUri(const char *text);

  check whether it's a uri or not by checking special uri characters

    static void navigate(AppState *state, const char *text);

  navigate to the uri that the user has typed and also initiate isUri checking to determine whether it should navigate to user input as a uri or just google search

    static void onUriActive(GtkEntry *entry, gpointer userData);

  after data is passed to navigate gtk will then change the focus back to webview

    static void onSyncUri(WebKitWebView *wv, GParamSpec *ps, gpointer userData);

  supposed to display current webpage uri but it's still doesn't support tab switching yet

    static void onFocus(GtkEventControllerFocus *focus, gpointer userData);

  move focus to the uri bar

    void syncSearch(UriBarData *d, const char *uri);

  sync the text displayed with the current uri. still need to fix the fact that it will only do this when it was typed directly in the search bar and not going to automatically take a page uri every time it gets into a new page

    GtkWidget *makeUriSearch(AppState *state);

  create a box to input uri or search for something 

    static void onActivate(GtkEntry *entry, gpointer userData);

  take text entry and pass it to navigate 

## spotlight.c

  handle spotlight search that pretty much work like search but Kooler

### Functions

    static gboolean onSpotlightKey(GtkEventControllerKey *key, guint keyvalue, guint keycode, GdkModifierType state, gpointer userData);

  close spotlight on escape

    static void onNotCardClick(GtkGestureClick *gesture, int nPress, double x, double y, gpointer userData);

  close if there is any click outside of the box

    GtkWidget *makeSpotlight(AppState *state);

  well this one i quite obvious isn't it?

    void showSpotlight(GtkWidget *spotlight);

  blur background and set spotlight box to visible and take the focus to entry immediately

    
    
