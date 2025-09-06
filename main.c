/**
 * ytdl.c
 *
 *   - Tetsuo - c/asm
 *   - https://www.github.com/7etsuo
 *   - https://www.x.com/tetsuoai
 *
 * ---------------------------------------------------------------------------
 *
 * Description:
 *   ytdl is a command-line utility for downloading YouTube videos using
 *   yt-dlp. It allows users to specify the output directory& select video
 *   formats. The program fetches video information in JSON format, parses
 *   available formats, and enables users to choose their preferred format
 *   for downloading.
 *
 * Features:
 *   - Fetch video metadata using yt-dlp in JSON format.
 *   - Parse and display available video formats with details such as format
 *     code, resolution, extension, and filesize.
 *   - Prompt users to select a desired video format or default to the best
 *     quality available.
 *   - Download the selected video format to a specified or current directory.
 *
 * Usage:
 *   ./ytdl [OPTIONS] URL
 *
 *   Options:
 *     -h, --help            Display this help message and exit.
 *     -o, --output PATH     Specify the output directory for downloaded
 * videos. Defaults to the current working directory if not provided.
 *
 *   Examples:
 *     - Display help message:
 *         ./ytdl --help
 *
 *     - Download a video to the current directory:
 *         ./ytdl https://www.youtube.com/watch?v=example
 *
 *     - Download a video to a specified directory:
 *         ./ytdl -o /path/to/download https://www.youtube.com/watch?v=example
 *
 * Dependencies:
 *   - yt-dlp: Ensure that yt-dlp is installed
 * https://github.com/yt-dlp/yt-dlp.
 *   - jansson: A C library for encoding, decoding, and manipulating JSON data.
 *             Installation:
 *               On Debian/Ubuntu:
 *                 sudo apt-get install libjansson-dev
 *               On macOS (using Homebrew):
 *                 brew install jansson
 *
 * Compilation:
 *   To compile the program: make all
 *
 * https://www.x.com/tetsuoai
 * ---------------------------------------------------------------------------
 */

#include "argument_parsing.h"
#include "command_execution.h"
#include "directory_management.h"
#include "download_helpers.h"
#include "format_parsing.h"
#include "help_display.h"
#include "user_interaction.h"
#include "video_info.h"
#include "ytdl.h"

#if USE_NCURSES
#include "terminal_ui.h"
// Global UI state for progress tracking
UIState *g_current_ui_state = NULL;
DownloadProgress *g_current_progress = NULL;
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/**
 * Validate configuration structure for required fields.
 * @param config Configuration structure to validate
 * @return 0 if valid, -1 if invalid
 */
static int
validate_config (const Config *config)
{
  if (config == NULL)
    {
      fprintf (stderr, "Error: Configuration is NULL\n");
      return -1;
    }

  if (config->url == NULL)
    {
      fprintf (stderr, "Error: URL is not set\n");
      return -1;
    }

  if (config->output_path == NULL)
    {
      fprintf (stderr, "Error: Output path is not set\n");
      return -1;
    }

  // Validate URL length
  size_t url_len = strlen (config->url);
  if (url_len == 0 || url_len >= MAX_URL_LENGTH)
    {
      fprintf (stderr, "Error: Invalid URL length (%zu)\n", url_len);
      return -1;
    }

  // Validate output path length
  size_t path_len = strlen (config->output_path);
  if (path_len == 0 || path_len >= MAX_PATH_LENGTH)
    {
      fprintf (stderr, "Error: Invalid output path length (%zu)\n", path_len);
      return -1;
    }

  return 0;
}

/**
 * Display configuration information safely.
 * @param config Configuration structure to display
 * @return 0 on success, -1 on error
 */
static int
display_config_info (const Config *config)
{
  if (printf ("URL: %s\n", config->url) < 0)
    {
      fprintf (stderr, "Error: Failed to display URL\n");
      return -1;
    }

  if (printf ("Output path: %s\n", config->output_path) < 0)
    {
      fprintf (stderr, "Error: Failed to display output path\n");
      return -1;
    }

  return 0;
}

/**
 * Main application entry point with comprehensive error handling.
 * @param argc Argument count
 * @param argv Argument vector
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error
 */
int
main (int argc, char *argv[])
{
  // Initialize configuration structure explicitly
  Config config = { .url = NULL, .output_path = NULL };

  int result = EXIT_FAILURE; // Default to failure

  // Parse command line arguments
  if (parse_arguments (argc, argv, &config) != EXIT_SUCCESS)
    {
      goto cleanup;
    }

  // Initialize output path
  if (initialize_output_path (&config) != EXIT_SUCCESS)
    {
      goto cleanup;
    }

  // Validate configuration
  if (validate_config (&config) != 0)
    {
      goto cleanup;
    }

  // Display configuration information
  if (display_config_info (&config) != 0)
    {
      goto cleanup;
    }

#if USE_NCURSES
  // Initialize terminal UI if available
  UIState ui_state;
  bool use_ui = false;

  if (ui_init (&ui_state) == 0)
    {
      use_ui = true;
      // Display initial status
      ui_show_status (&ui_state, "Fetching video information...");
    }
#endif

  // Get video information
  char *json_str = get_video_info (config.url);
  if (json_str == NULL)
    {
      fprintf (stderr, "Error: Failed to retrieve video information\n");
#if USE_NCURSES
      if (use_ui)
        {
          ui_show_error (&ui_state, "Failed to retrieve video information");
          sleep (2);
          ui_cleanup (&ui_state);
        }
#endif
      goto cleanup;
    }

  // Parse video formats
  json_t *formats = parse_formats (json_str);

  // Extract video info for UI display
#if USE_NCURSES
  VideoDisplayInfo video_info = { 0 };
  bool video_info_allocated = false;
  if (use_ui && formats != NULL)
    {
      // Parse video title and other info from JSON
      json_error_t error;
      json_t *root = json_loads (json_str, 0, &error);
      if (root)
        {
          json_t *title_obj = json_object_get (root, "title");
          json_t *channel_obj = json_object_get (root, "channel");
          json_t *duration_obj = json_object_get (root, "duration");

          if (title_obj && json_is_string (title_obj))
            {
              video_info.title = strdup (json_string_value (title_obj));
              video_info_allocated = true;
            }
          if (channel_obj && json_is_string (channel_obj))
            {
              video_info.channel = strdup (json_string_value (channel_obj));
              video_info_allocated = true;
            }
          if (duration_obj && json_is_integer (duration_obj))
            {
              int duration_seconds = json_integer_value (duration_obj);
              video_info.duration = malloc (32);
              if (video_info.duration)
                {
                  ui_format_time (duration_seconds, video_info.duration, 32);
                  video_info_allocated = true;
                }
            }

          json_decref (root);
        }
    }
#endif

  free (json_str); // Free immediately after parsing
  json_str = NULL;

  if (formats == NULL)
    {
      fprintf (stderr, "Error: Failed to parse video formats\n");
#if USE_NCURSES
      if (use_ui)
        {
          ui_show_error (&ui_state, "Failed to parse video formats");
          sleep (2);
          ui_cleanup (&ui_state);
        }
#endif
      goto cleanup;
    }

  char *format_code = NULL;

#if USE_NCURSES
  if (use_ui)
    {
      // Display video info
      ui_display_video_info (&ui_state, &video_info);

      // Interactive format selection
      FormatListState list_state = { 0 };
      ui_display_formats (&ui_state, formats, &list_state);
      format_code = ui_select_format_interactive (&ui_state, &list_state);

      if (format_code == NULL)
        {
          ui_show_status (&ui_state, "Download cancelled");
          sleep (1);
        }
    }
  else
    {
#endif
      // Fallback to text-based display
      display_formats (formats);
      json_decref (formats);
      formats = NULL;

      // Prompt user for format selection
      format_code = prompt_for_format ();
#if USE_NCURSES
    }
#endif

  if (format_code == NULL)
    {
      fprintf (stderr, "Error: Failed to get format selection from user\n");
#if USE_NCURSES
      if (use_ui)
        {
          ui_cleanup (&ui_state);
        }
#endif
      goto cleanup;
    }

#if USE_NCURSES
  // Cleanup formats if using UI (already cleaned up in text mode)
  if (use_ui && formats != NULL)
    {
      json_decref (formats);
      formats = NULL;
    }
#endif

    // Download the video
#if USE_NCURSES
  if (use_ui)
    {
      // Show download progress UI
      DownloadProgress progress = { 0 };
      progress.start_time = time (NULL);
      strcpy (progress.current_stage, "Starting download...");
      ui_show_progress (&ui_state, &progress);

      // Set global pointers for progress tracking
      g_current_ui_state = &ui_state;
      g_current_progress = &progress;
    }
#endif

  if (download_video (&config, format_code) != EXIT_SUCCESS)
    {
      fprintf (stderr, "Error: Video download failed\n");
#if USE_NCURSES
      if (use_ui)
        {
          ui_show_error (&ui_state, "Video download failed");
          sleep (2);
        }
#endif
      free (format_code);
      goto cleanup;
    }

#if USE_NCURSES
  if (use_ui)
    {
      ui_show_status (&ui_state, "Download complete!");
      sleep (1);
    }
#endif

  // Success path
  free (format_code);
  result = EXIT_SUCCESS;

cleanup:
#if USE_NCURSES
  if (use_ui)
    {
      ui_cleanup (&ui_state);
      // Free video info only if allocated
      if (video_info_allocated)
        {
          free (video_info.title);
          free (video_info.channel);
          free (video_info.duration);
        }
    }
#endif
  cleanup (&config);
  return result;
}
