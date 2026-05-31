CC     = gcc
CFLAGS = $(shell pkg-config --cflags gtk4 webkitgtk-6.0)
LIBS   = $(shell pkg-config --libs gtk4 webkitgtk-6.0) -lm

SRCS = main.c \
       tabs/tabs.c \
       webview/webview.c \
       sidebar/sidebar.c \
       toolbar/toolbar.c\
       search/search.c\
       css/css.c

OBJS = $(SRCS:.c=.o)

webkit : $(OBJS)
	$(CC) $(OBJS) $(LIBS) -o webkit

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) webkit
