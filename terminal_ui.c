#include "terminal_ui.h"
#include "format_parsing.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

// Global UI state for signal handlers
static UIState *g_ui_state = NULL;
static struct termios saved_termios;
static bool termios_saved = false;

/**
 * Initialize color pairs for the terminal UI.
 * @param state UI state structure
 */
static void
init_colors (UIState *state)
{
  if (!has_colors () || !state->colors_supported)
    {
      return;
    }

  start_color ();
  use_default_colors ();

  // Initialize color pairs
  init_pair (COLOR_PAIR_DEFAULT, COLOR_WHITE, -1);
  init_pair (COLOR_PAIR_HEADER, COLOR_WHITE, COLOR_BLUE);
  init_pair (COLOR_PAIR_SUCCESS, COLOR_GREEN, -1);
  init_pair (COLOR_PAIR_ERROR, COLOR_RED, -1);
  init_pair (COLOR_PAIR_WARNING, COLOR_YELLOW, -1);
  init_pair (COLOR_PAIR_SELECTED, COLOR_BLACK, COLOR_CYAN);
  init_pair (COLOR_PAIR_HIGHLIGHT, COLOR_WHITE, COLOR_MAGENTA);
  init_pair (COLOR_PAIR_PROGRESS_FILLED, COLOR_WHITE, COLOR_GREEN);
  init_pair (COLOR_PAIR_PROGRESS_EMPTY, COLOR_WHITE, COLOR_BLACK);
  init_pair (COLOR_PAIR_BORDER, COLOR_CYAN, -1);

  state->color_pairs = 10;
}

/**
 * Create UI windows based on terminal size.
 * @param state UI state structure
 * @return 0 on success, -1 on error
 */
static int
create_windows (UIState *state)
{
  // Get terminal dimensions
  getmaxyx (stdscr, state->term_height, state->term_width);

  // Create header window (3 lines at top)
  state->header_window = newwin (4, state->term_width, 0, 0);
  if (state->header_window == NULL)
    {
      return -1;
    }

  // Create status window (1 line at bottom)
  state->status_window
      = newwin (1, state->term_width, state->term_height - 1, 0);
  if (state->status_window == NULL)
    {
      delwin (state->header_window);
      return -1;
    }

  // Create content window (remaining space)
  int content_height = state->term_height - 5;
  state->content_window = newwin (content_height, state->term_width, 4, 0);
  if (state->content_window == NULL)
    {
      delwin (state->header_window);
      delwin (state->status_window);
      return -1;
    }

  // Create panels for proper layering
  state->header_panel = new_panel (state->header_window);
  state->content_panel = new_panel (state->content_window);
  state->status_panel = new_panel (state->status_window);

  // Enable keypad for arrow keys
  keypad (state->content_window, TRUE);

  return 0;
}

/**
 * Signal handler for terminal UI events.
 * @param sig Signal number
 */
void
ui_signal_handler (int sig)
{
  if (g_ui_state == NULL)
    {
      return;
    }

  switch (sig)
    {
    case SIGWINCH:
      g_ui_state->resize_pending = 1;
      break;
    case SIGINT:
    case SIGTERM:
      g_ui_state->shutdown_pending = 1;
      break;
    }
}

/**
 * Setup signal handlers for UI.
 */
void
ui_setup_signals (void)
{
  struct sigaction sa;
  memset (&sa, 0, sizeof (sa));
  sa.sa_handler = ui_signal_handler;
  sigemptyset (&sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  sigaction (SIGWINCH, &sa, NULL);
  sigaction (SIGINT, &sa, NULL);
  sigaction (SIGTERM, &sa, NULL);
}

/**
 * Save terminal state for restoration on exit.
 */
static void
save_terminal_state (void)
{
  if (tcgetattr (STDIN_FILENO, &saved_termios) == 0)
    {
      termios_saved = true;
    }
}

/**
 * Restore terminal state on exit.
 */
static void
restore_terminal_state (void)
{
  if (termios_saved)
    {
      tcsetattr (STDIN_FILENO, TCSANOW, &saved_termios);
    }
  // Exit alternate screen buffer
  printf ("\033[?1049l");
  // Reset colors
  printf ("\033[0m");
  fflush (stdout);
}

/**
 * Initialize the terminal UI system.
 * @param state UI state structure to initialize
 * @return 0 on success, -1 on error
 */
int
ui_init (UIState *state)
{
  if (state == NULL)
    {
      return -1;
    }

  // Initialize state
  memset (state, 0, sizeof (UIState));

  // Initialize mutex
  if (pthread_mutex_init (&state->ui_mutex, NULL) != 0)
    {
      return -1;
    }

  // Save terminal state
  save_terminal_state ();

  // Set locale for UTF-8 support
  setlocale (LC_ALL, "");

  // Initialize ncurses
  if (initscr () == NULL)
    {
      pthread_mutex_destroy (&state->ui_mutex);
      return -1;
    }

  state->ncurses_available = true;

  // Setup ncurses options
  cbreak ();                       // Disable line buffering
  noecho ();                       // Don't echo keypresses
  curs_set (0);                    // Hide cursor
  keypad (stdscr, TRUE);           // Enable keypad on stdscr too
  nodelay (stdscr, TRUE);          // Non-blocking input
  timeout (UI_UPDATE_INTERVAL_MS); // Input timeout

  // Check color support
  state->colors_supported = has_colors ();
  if (state->colors_supported)
    {
      state->max_colors = COLORS;
      init_colors (state);
    }

  // Create windows
  if (create_windows (state) != 0)
    {
      endwin ();
      pthread_mutex_destroy (&state->ui_mutex);
      return -1;
    }

  // Setup signal handlers
  g_ui_state = state;
  ui_setup_signals ();

  // Clear and refresh
  clear ();
  refresh ();

  return 0;
}

/**
 * Cleanup and shutdown the terminal UI system.
 * @param state UI state structure
 */
void
ui_cleanup (UIState *state)
{
  if (state == NULL || !state->ncurses_available)
    {
      return;
    }

  // Remove signal handlers
  g_ui_state = NULL;

  // Delete panels
  if (state->status_panel)
    del_panel (state->status_panel);
  if (state->content_panel)
    del_panel (state->content_panel);
  if (state->header_panel)
    del_panel (state->header_panel);

  // Delete windows
  if (state->status_window)
    delwin (state->status_window);
  if (state->content_window)
    delwin (state->content_window);
  if (state->header_window)
    delwin (state->header_window);

  // End ncurses
  endwin ();

  // Restore terminal
  restore_terminal_state ();

  // Destroy mutex
  pthread_mutex_destroy (&state->ui_mutex);

  state->ncurses_available = false;
}

/**
 * Handle terminal resize events.
 * @param state UI state structure
 */
void
ui_handle_resize (UIState *state)
{
  if (!state->resize_pending)
    {
      return;
    }

  ui_lock (state);

  // End and refresh ncurses
  endwin ();
  refresh ();
  clear ();

  // Delete old panels and windows
  del_panel (state->status_panel);
  del_panel (state->content_panel);
  del_panel (state->header_panel);
  delwin (state->status_window);
  delwin (state->content_window);
  delwin (state->header_window);

  // Recreate windows with new size
  create_windows (state);

  // Update panels
  update_panels ();
  doupdate ();

  state->resize_pending = 0;

  ui_unlock (state);
}

/**
 * Thread-safe lock for UI operations.
 * @param state UI state structure
 */
void
ui_lock (UIState *state)
{
  if (state != NULL)
    {
      pthread_mutex_lock (&state->ui_mutex);
    }
}

/**
 * Thread-safe unlock for UI operations.
 * @param state UI state structure
 */
void
ui_unlock (UIState *state)
{
  if (state != NULL)
    {
      pthread_mutex_unlock (&state->ui_mutex);
    }
}

/**
 * Display video information in the header window.
 * @param state UI state structure
 * @param info Video information to display
 */
void
ui_display_video_info (UIState *state, const VideoDisplayInfo *info)
{
  if (state == NULL || info == NULL || !state->ncurses_available)
    {
      return;
    }

  ui_lock (state);

  WINDOW *win = state->header_window;

  // Clear header
  werase (win);

  // Draw border
  if (state->colors_supported)
    {
      wattron (win, COLOR_PAIR (COLOR_PAIR_BORDER));
    }
  box (win, 0, 0);
  if (state->colors_supported)
    {
      wattroff (win, COLOR_PAIR (COLOR_PAIR_BORDER));
    }

  // Draw title bar
  if (state->colors_supported)
    {
      wattron (win, COLOR_PAIR (COLOR_PAIR_HEADER) | A_BOLD);
    }
  mvwprintw (win, 0, 2, " YouTube Video Downloader v2.0 ");
  if (state->colors_supported)
    {
      wattroff (win, COLOR_PAIR (COLOR_PAIR_HEADER) | A_BOLD);
    }

  // Video title (truncate if too long)
  int max_title_len = state->term_width - 20;
  char truncated_title[256];
  if (info->title && strlen (info->title) > (size_t)max_title_len)
    {
      snprintf (truncated_title, sizeof (truncated_title), "%.250s...",
                info->title);
    }
  else
    {
      snprintf (truncated_title, sizeof (truncated_title), "%s",
                info->title ? info->title : "N/A");
    }

  mvwprintw (win, 1, 2, "Video: %s", truncated_title);

  // Channel and duration
  mvwprintw (win, 2, 2, "Channel: %-30s Duration: %s",
             info->channel ? info->channel : "N/A",
             info->duration ? info->duration : "N/A");

  wrefresh (win);
  ui_unlock (state);
}

/**
 * Format bytes into human-readable string.
 * @param bytes Number of bytes
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 */
void
ui_format_bytes (long long bytes, char *buffer, size_t buffer_size)
{
  const char *units[] = { "B", "KB", "MB", "GB", "TB" };
  int unit_index = 0;
  double size = (double)bytes;

  while (size >= 1024.0 && unit_index < 4)
    {
      size /= 1024.0;
      unit_index++;
    }

  if (unit_index == 0)
    {
      snprintf (buffer, buffer_size, "%lld %s", bytes, units[0]);
    }
  else
    {
      snprintf (buffer, buffer_size, "%.1f %s", size, units[unit_index]);
    }
}

/**
 * Format seconds into human-readable time string.
 * @param seconds Number of seconds
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 */
void
ui_format_time (int seconds, char *buffer, size_t buffer_size)
{
  if (seconds < 60)
    {
      snprintf (buffer, buffer_size, "%ds", seconds);
    }
  else if (seconds < 3600)
    {
      snprintf (buffer, buffer_size, "%dm %ds", seconds / 60, seconds % 60);
    }
  else
    {
      snprintf (buffer, buffer_size, "%dh %dm", seconds / 3600,
                (seconds % 3600) / 60);
    }
}

/**
 * Show status message in the status bar.
 * @param state UI state structure
 * @param status_msg Status message to display
 */
void
ui_show_status (UIState *state, const char *status_msg)
{
  if (state == NULL || !state->ncurses_available)
    {
      return;
    }

  ui_lock (state);

  WINDOW *win = state->status_window;
  werase (win);

  // Draw status message
  if (state->colors_supported)
    {
      wattron (win, COLOR_PAIR (COLOR_PAIR_DEFAULT));
    }
  mvwprintw (win, 0, 0, " %s", status_msg ? status_msg : "");

  // Draw keyboard shortcuts on the right
  const char *shortcuts
      = "[↑/↓] Navigate [Enter] Select [B] Best [Esc] Cancel";
  int x_pos = state->term_width - strlen (shortcuts) - 2;
  if (x_pos > 0)
    {
      mvwprintw (win, 0, x_pos, "%s ", shortcuts);
    }

  if (state->colors_supported)
    {
      wattroff (win, COLOR_PAIR (COLOR_PAIR_DEFAULT));
    }

  wrefresh (win);
  ui_unlock (state);
}

/**
 * Show error message.
 * @param state UI state structure
 * @param error_msg Error message to display
 */
void
ui_show_error (UIState *state, const char *error_msg)
{
  if (state == NULL || !state->ncurses_available)
    {
      // Fall back to stderr
      fprintf (stderr, "Error: %s\n", error_msg);
      return;
    }

  ui_lock (state);

  WINDOW *win = state->status_window;
  werase (win);

  if (state->colors_supported)
    {
      wattron (win, COLOR_PAIR (COLOR_PAIR_ERROR) | A_BOLD);
    }
  mvwprintw (win, 0, 0, " ERROR: %s", error_msg ? error_msg : "Unknown error");
  if (state->colors_supported)
    {
      wattroff (win, COLOR_PAIR (COLOR_PAIR_ERROR) | A_BOLD);
    }

  wrefresh (win);
  ui_unlock (state);
}
