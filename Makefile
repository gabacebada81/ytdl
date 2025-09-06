CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11

# ncurses detection
NCURSES_CFLAGS := $(shell pkg-config --cflags ncursesw 2>/dev/null || pkg-config --cflags ncurses 2>/dev/null)
NCURSES_LIBS := $(shell pkg-config --libs ncursesw 2>/dev/null || pkg-config --libs ncurses 2>/dev/null)

# Add ncurses flags if available
ifneq ($(NCURSES_LIBS),)
    CFLAGS += $(NCURSES_CFLAGS) -DUSE_NCURSES=1
    LDFLAGS = -ljansson $(NCURSES_LIBS) -lpanel -lpthread
else
    CFLAGS += -DUSE_NCURSES=0
    LDFLAGS = -ljansson
endif

SRCS = main.c command_execution.c video_info.c format_parsing.c user_interaction.c directory_management.c download_helpers.c argument_parsing.c help_display.c terminal_ui.c ui_format_display.c ui_progress.c
OBJS = $(SRCS:.c=.o)
TARGET = ytdl

.PHONY: all clean check_ncurses

all: check_ncurses $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

check_ncurses:
	@if [ -z "$(NCURSES_LIBS)" ]; then \
		echo "Warning: ncurses not found. Building without terminal UI support."; \
		echo "Install with: sudo apt-get install libncurses5-dev libncursesw5-dev"; \
	fi

clean:
	rm -f $(OBJS) $(TARGET)