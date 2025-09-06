#ifndef TERMINAL_UI_H
#define TERMINAL_UI_H

#include "ytdl.h"
#include <locale.h>
#include <ncurses.h>
#include <panel.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>

// Maximum number of visible formats in the list
#define MAX_VISIBLE_FORMATS 20
#define SPEED_SAMPLE_SIZE 10
#define UI_UPDATE_INTERVAL_MS 100

// Color pair definitions
enum UIColorPairs
{
  COLOR_PAIR_DEFAULT = 1,
  COLOR_PAIR_HEADER,
  COLOR_PAIR_SUCCESS,
  COLOR_PAIR_ERROR,
  COLOR_PAIR_WARNING,
  COLOR_PAIR_SELECTED,
  COLOR_PAIR_HIGHLIGHT,
  COLOR_PAIR_PROGRESS_FILLED,
  COLOR_PAIR_PROGRESS_EMPTY,
  COLOR_PAIR_BORDER
};

// UI State structure
typedef struct
{
  bool ncurses_available;
  bool colors_supported;
  int max_colors;
  int color_pairs;
  WINDOW *header_window;
  WINDOW *content_window;
  WINDOW *status_window;
  PANEL *header_panel;
  PANEL *content_panel;
  PANEL *status_panel;
  int term_height;
  int term_width;
  pthread_mutex_t ui_mutex;
  volatile sig_atomic_t resize_pending;
  volatile sig_atomic_t shutdown_pending;
} UIState;

// Video information for display
typedef struct
{
  char *title;
  char *channel;
  char *duration;
  char *view_count;
  char *upload_date;
} VideoDisplayInfo;

// Format list state for scrolling
typedef struct
{
  int visible_start;
  int visible_lines;
  int selected_index;
  int total_formats;
  json_t *formats;
  WINDOW *pad;
  int pad_height;
} FormatListState;

// Download progress information
typedef struct
{
  long long downloaded_bytes;
  long long total_bytes;
  double download_speed;
  time_t start_time;
  time_t estimated_completion;
  char current_stage[256];
  // Speed calculation ring buffer
  struct
  {
    time_t timestamp;
    long long bytes;
  } samples[SPEED_SAMPLE_SIZE];
  int sample_index;
  time_t last_update;
} DownloadProgress;

// UI Message for thread communication
typedef enum
{
  MSG_PROGRESS,
  MSG_ERROR,
  MSG_STATUS,
  MSG_COMPLETE
} UIMessageType;

typedef struct
{
  UIMessageType type;
  union
  {
    DownloadProgress progress;
    char error_msg[256];
    char status_msg[256];
  } data;
} UIMessage;

// Function declarations
int ui_init (UIState *state);
void ui_cleanup (UIState *state);
void ui_handle_resize (UIState *state);
void ui_display_video_info (UIState *state, const VideoDisplayInfo *info);
int ui_display_formats (UIState *state, json_t *formats,
                        FormatListState *list_state);
char *ui_select_format_interactive (UIState *state,
                                    FormatListState *list_state);
void ui_show_progress (UIState *state, const DownloadProgress *progress);
void ui_show_error (UIState *state, const char *error_msg);
void ui_show_status (UIState *state, const char *status_msg);
void ui_update_progress (DownloadProgress *progress, long long downloaded,
                         long long total);
double ui_calculate_speed (DownloadProgress *progress);
void ui_format_bytes (long long bytes, char *buffer, size_t buffer_size);
void ui_format_time (int seconds, char *buffer, size_t buffer_size);

// Signal handlers
void ui_signal_handler (int sig);
void ui_setup_signals (void);

// Thread-safe UI operations
void ui_lock (UIState *state);
void ui_unlock (UIState *state);

#endif // TERMINAL_UI_H
