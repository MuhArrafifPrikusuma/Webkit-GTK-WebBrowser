CC     = gcc
CFLAGS = $(shell pkg-config --cflags gtk4 webkitgtk-6.0)
LIBS   = $(shell pkg-config --libs gtk4 webkitgtk-6.0)

SRCS = main.c \
       tabs/tabs.c \
       webview/webview.c \
       sidebar/sidebar.c \
       css/css.c

OBJS = $(SRCS:.c=.o)

browser: $(OBJS)
	$(CC) $(OBJS) $(LIBS) -o browser

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) browser
