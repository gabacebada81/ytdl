#include "terminal_ui.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * Draw a progress bar.
 * @param win Window to draw in
 * @param y Y position
 * @param x X position
 * @param width Width of progress bar
 * @param percent Percentage complete (0-100)
 * @param colors_supported Whether colors are supported
 */
static void
draw_progress_bar (WINDOW *win, int y, int x, int width, double percent,
                   bool colors_supported)
{
  int filled = (int)((percent / 100.0) * width);
  if (filled > width)
    filled = width;
  if (filled < 0)
    filled = 0;

  mvwprintw (win, y, x, "[");

  // Draw filled portion
  if (colors_supported)
    {
      wattron (win, COLOR_PAIR (COLOR_PAIR_PROGRESS_FILLED));
    }
  for (int i = 0; i < filled; i++)
    {
      waddch (win, ACS_BLOCK);
    }
  if (colors_supported)
    {
      wattroff (win, COLOR_PAIR (COLOR_PAIR_PROGRESS_FILLED));
    }

  // Draw empty portion
  if (colors_supported)
    {
      wattron (win, COLOR_PAIR (COLOR_PAIR_PROGRESS_EMPTY));
    }
  for (int i = filled; i < width; i++)
    {
      waddch (win, ' ');
    }
  if (colors_supported)
    {
      wattroff (win, COLOR_PAIR (COLOR_PAIR_PROGRESS_EMPTY));
    }

  wprintw (win, "] %3.0f%%", percent);
}

/**
 * Calculate download speed using ring buffer.
 * @param progress Progress structure
 * @return Speed in bytes per second
 */
double
ui_calculate_speed (DownloadProgress *progress)
{
  if (progress == NULL || progress->sample_index == 0)
    {
      return 0.0;
    }

  // Find oldest and newest samples
  int oldest_idx = 0;
  int newest_idx
      = (progress->sample_index - 1 + SPEED_SAMPLE_SIZE) % SPEED_SAMPLE_SIZE;

  // Need at least 2 samples
  if (progress->samples[oldest_idx].timestamp == 0
      || progress->samples[newest_idx].timestamp == 0)
    {
      return 0.0;
    }

  // Calculate time difference
  double time_diff = difftime (progress->samples[newest_idx].timestamp,
                               progress->samples[oldest_idx].timestamp);
  if (time_diff <= 0)
    {
      return 0.0;
    }

  // Calculate byte difference
  long long byte_diff = progress->samples[newest_idx].bytes
                        - progress->samples[oldest_idx].bytes;
  if (byte_diff < 0)
    {
      return 0.0;
    }

  return (double)byte_diff / time_diff;
}

/**
 * Update progress information with new data.
 * @param progress Progress structure to update
 * @param downloaded Bytes downloaded so far
 * @param total Total bytes to download
 */
void
ui_update_progress (DownloadProgress *progress, long long downloaded,
                    long long total)
{
  if (progress == NULL)
    {
      return;
    }

  progress->downloaded_bytes = downloaded;
  progress->total_bytes = total;

  // Update speed calculation ring buffer
  time_t now = time (NULL);
  if (now != progress->samples[progress->sample_index].timestamp)
    {
      progress->sample_index
          = (progress->sample_index + 1) % SPEED_SAMPLE_SIZE;
      progress->samples[progress->sample_index].timestamp = now;
      progress->samples[progress->sample_index].bytes = downloaded;
    }

  // Calculate speed
  progress->download_speed = ui_calculate_speed (progress);

  // Calculate ETA
  if (progress->download_speed > 0 && total > downloaded)
    {
      long long remaining = total - downloaded;
      int eta_seconds = (int)(remaining / progress->download_speed);
      progress->estimated_completion = now + eta_seconds;
    }

  progress->last_update = now;
}

/**
 * Display download progress in the content window.
 * @param state UI state structure
 * @param progress Download progress information
 */
void
ui_show_progress (UIState *state, const DownloadProgress *progress)
{
  if (state == NULL || progress == NULL || !state->ncurses_available)
    {
      return;
    }

  ui_lock (state);

  WINDOW *win = state->content_window;
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

  // Draw title
  mvwprintw (win, 0, 2, " Download Progress ");

  int y = 2;
  int content_width = getmaxx (win) - 4;

  // Current stage
  if (strlen (progress->current_stage) > 0)
    {
      mvwprintw (win, y++, 2, "Stage: %s", progress->current_stage);
      y++;
    }

  // File size information
  char downloaded_str[32], total_str[32];
  ui_format_bytes (progress->downloaded_bytes, downloaded_str,
                   sizeof (downloaded_str));
  ui_format_bytes (progress->total_bytes, total_str, sizeof (total_str));

  mvwprintw (win, y++, 2, "Downloaded: %s / %s", downloaded_str, total_str);

  // Progress bar
  double percent = 0.0;
  if (progress->total_bytes > 0)
    {
      percent
          = (double)progress->downloaded_bytes / progress->total_bytes * 100.0;
    }

  y++;
  draw_progress_bar (win, y++, 2, content_width - 10, percent,
                     state->colors_supported);

  // Download speed
  if (progress->download_speed > 0)
    {
      char speed_str[32];
      ui_format_bytes ((long long)progress->download_speed, speed_str,
                       sizeof (speed_str));

      y++;
      mvwprintw (win, y++, 2, "Speed: %s/s", speed_str);

      // ETA
      if (progress->estimated_completion > 0)
        {
          time_t now = time (NULL);
          int eta_seconds
              = (int)difftime (progress->estimated_completion, now);
          if (eta_seconds > 0)
            {
              char eta_str[32];
              ui_format_time (eta_seconds, eta_str, sizeof (eta_str));
              mvwprintw (win, y++, 2, "ETA: %s", eta_str);
            }
        }
    }

  // Time elapsed
  if (progress->start_time > 0)
    {
      time_t now = time (NULL);
      int elapsed = (int)difftime (now, progress->start_time);
      char elapsed_str[32];
      ui_format_time (elapsed, elapsed_str, sizeof (elapsed_str));

      y++;
      mvwprintw (win, y++, 2, "Elapsed: %s", elapsed_str);
    }

  wrefresh (win);
  ui_unlock (state);
}

/**
 * Create an animated spinner for indeterminate progress.
 * @param frame Current animation frame
 * @return Spinner character
 */
static char
get_spinner_char (int frame)
{
  const char spinner[] = { '|', '/', '-', '\\' };
  return spinner[frame % 4];
}

/**
 * Show indeterminate progress (when total size is unknown).
 * @param state UI state structure
 * @param message Status message
 * @param frame Animation frame counter
 */
void
ui_show_indeterminate_progress (UIState *state, const char *message, int frame)
{
  if (state == NULL || !state->ncurses_available)
    {
      return;
    }

  ui_lock (state);

  WINDOW *win = state->content_window;
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

  // Draw title
  mvwprintw (win, 0, 2, " Processing ");

  // Draw message with spinner
  int y = getmaxy (win) / 2;
  int x = (getmaxx (win) - strlen (message) - 4) / 2;

  mvwprintw (win, y, x, "%s %c", message, get_spinner_char (frame));

  wrefresh (win);
  ui_unlock (state);
}
