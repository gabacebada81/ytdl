#include "download_helpers.h"
#include "command_execution.h"

#if USE_NCURSES
#include "terminal_ui.h"
extern UIState *g_current_ui_state;
extern DownloadProgress *g_current_progress;
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Default format code for best quality video
#define DEFAULT_FORMAT_CODE "bestvideo[ext=mp4]+bestaudio[ext=m4a]/best[ext=mp4]/best"
// yt-dlp command name
#define YT_DLP_COMMAND "yt-dlp"
// Output template format string
#define OUTPUT_TEMPLATE_FORMAT "%s/%%(title)s.%%(ext)s"

/**
 * Validate input parameters for download command building.
 * @param format_code Format code string (can be NULL)
 * @param output_path Output path string
 * @param url URL string
 * @return 0 if valid, -1 if invalid
 */
static int
validate_download_parameters(const char *format_code, const char *output_path, const char *url)
{
  if (output_path == NULL) {
    fprintf(stderr, "Error: Output path is NULL\n");
    return -1;
  }

  if (url == NULL) {
    fprintf(stderr, "Error: URL is NULL\n");
    return -1;
  }

  size_t output_path_len = strlen(output_path);
  if (output_path_len == 0 || output_path_len >= MAX_PATH_LENGTH) {
    fprintf(stderr, "Error: Invalid output path length (%zu)\n", output_path_len);
    return -1;
  }

  size_t url_len = strlen(url);
  if (url_len == 0 || url_len >= MAX_URL_LENGTH) {
    fprintf(stderr, "Error: Invalid URL length (%zu)\n", url_len);
    return -1;
  }

  if (format_code != NULL) {
    size_t format_len = strlen(format_code);
    if (format_len >= FORMAT_CODE_LENGTH) {
      fprintf(stderr, "Error: Format code too long (%zu >= %d)\n",
              format_len, FORMAT_CODE_LENGTH);
      return -1;
    }
  }

  return 0;
}

/**
 * Safely create output template string with bounds checking.
 * @param output_path Base output path
 * @return Allocated output template string, NULL on error
 */
static char *
create_output_template(const char *output_path)
{
  // Calculate required length: path + template + null terminator
  size_t required_len = strlen(output_path) + strlen(OUTPUT_TEMPLATE_FORMAT) + 1;

  if (required_len >= MAX_PATH_LENGTH) {
    fprintf(stderr, "Error: Output template would exceed maximum path length\n");
    return NULL;
  }

  char *output_template = malloc(MAX_PATH_LENGTH);
  if (output_template == NULL) {
    fprintf(stderr, "Error: Memory allocation failed for output template\n");
    return NULL;
  }

  int result = snprintf(output_template, MAX_PATH_LENGTH, OUTPUT_TEMPLATE_FORMAT, output_path);
  if (result < 0 || (size_t)result >= MAX_PATH_LENGTH) {
    fprintf(stderr, "Error: Failed to create output template\n");
    free(output_template);
    return NULL;
  }

  return output_template;
}

/**
 * Build command line arguments array for yt-dlp download.
 * @param format_code Format code (NULL for default)
 * @param output_path Output directory path
 * @param url Video URL
 * @return Allocated NULL-terminated argument array, NULL on error
 */
char **
build_download_command_args(const char *format_code, const char *output_path, const char *url)
{
  if (validate_download_parameters(format_code, output_path, url) == -1) {
    return NULL;
  }

  // Determine argument count based on format code presence
  int has_format = (format_code != NULL && strlen(format_code) > 0);
  int arg_count = has_format ? 6 : 5; // command + args + NULL

  char **args = malloc(sizeof(char *) * (arg_count + 1));
  if (args == NULL) {
    fprintf(stderr, "Error: Memory allocation failed for command arguments\n");
    return NULL;
  }

  // Initialize all pointers to NULL for safety
  memset(args, 0, sizeof(char *) * (arg_count + 1));

  int idx = 0;
  args[idx++] = YT_DLP_COMMAND;

  // Add format arguments
  args[idx++] = "-f";
  if (has_format) {
    args[idx++] = (char *)format_code;
  } else {
    args[idx++] = DEFAULT_FORMAT_CODE;
  }

  // Add output arguments
  args[idx++] = "-o";

  char *output_template = create_output_template(output_path);
  if (output_template == NULL) {
    free(args);
    return NULL;
  }
  args[idx++] = output_template;

  // Add URL
  args[idx++] = (char *)url;
  args[idx] = NULL; // NULL terminate

  return args;
}

/**
 * Free command arguments array and associated memory.
 * @param args Argument array to free
 */
void
free_command_args(char **args)
{
  if (args == NULL) {
    return;
  }

  // Free the output template (always at index 4 when present)
  if (args[4] != NULL) {
    free(args[4]);
  }

  free(args);
}

/**
 * Download video using yt-dlp with specified configuration.
 * @param config Configuration structure containing URL and output path
 * @param format_code Format code (NULL for default)
 * @return 0 on success, -1 on error
 */
int
download_video(const Config *config, const char *format_code)
{
  if (config == NULL) {
    fprintf(stderr, "Error: Configuration is NULL\n");
    return -1;
  }

#if USE_NCURSES
  if (g_current_ui_state && g_current_ui_state->ncurses_available) {
    // UI mode - show progress
    if (g_current_progress) {
      strcpy(g_current_progress->current_stage, "Preparing download...");
      ui_show_progress(g_current_ui_state, g_current_progress);
    }
  } else {
#endif
    printf("Downloading...\n");
#if USE_NCURSES
  }
#endif

  char **args = build_download_command_args(format_code, config->output_path, config->url);
  if (args == NULL) {
    fprintf(stderr, "Error: Failed to build download command arguments\n");
    return -1;
  }

  // Execute download command, handling ncurses mode properly
  int result;
#if USE_NCURSES
  if (g_current_ui_state && g_current_ui_state->ncurses_available) {
    // In ncurses mode, temporarily exit ncurses to prevent UI corruption
    def_prog_mode();    // Save current ncurses state
    endwin();           // Exit ncurses mode

    result = execute_command_without_output(YT_DLP_COMMAND, args);

    reset_prog_mode();  // Restore ncurses state
    refresh();          // Refresh the screen
  } else {
    result = execute_command_without_output(YT_DLP_COMMAND, args);
  }
#else
  result = execute_command_without_output(YT_DLP_COMMAND, args);
#endif

  if (result == 0) {
#if USE_NCURSES
    if (g_current_ui_state && g_current_ui_state->ncurses_available) {
      // UI mode - update progress to complete
      if (g_current_progress) {
        g_current_progress->downloaded_bytes = g_current_progress->total_bytes;
        strcpy(g_current_progress->current_stage, "Download complete!");
        ui_show_progress(g_current_ui_state, g_current_progress);
      }
    } else {
#endif
      printf("Download complete! Saved to: %s\n", config->output_path);
#if USE_NCURSES
    }
#endif
  } else {
    fprintf(stderr, "Error: Download failed with exit code %d\n", result);
  }

  free_command_args(args);
  return result;
}