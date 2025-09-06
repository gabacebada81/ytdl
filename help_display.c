#include "help_display.h"
#include "directory_management.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Help text constants for better maintainability
#define PROGRAM_NAME "ytdl"
#define USAGE_FORMAT "Usage: %s [OPTION]... URL\n"
#define DESCRIPTION "Download videos from YouTube using yt-dlp\n\n"
#define HELP_OPTION "  -h, --help\t\t\tDisplay this help message\n"
#define OUTPUT_OPTION                                                         \
  "  -o, --output PATH\t\tSpecify the output directory (default: current "    \
  "directory)\n"

/**
 * Display help information for the program.
 */
void
display_help (void)
{
  printf (USAGE_FORMAT, PROGRAM_NAME);
  printf (DESCRIPTION);
  printf ("Options:\n");
  printf (HELP_OPTION);
  printf (OUTPUT_OPTION);
}

/**
 * Clean up dynamically allocated resources in the configuration.
 * @param config Configuration structure to clean up
 *
 * Note: This function frees the output_path if it was dynamically allocated.
 * The url field is const and should not be freed here.
 * The output_path is always dynamically allocated in this codebase:
 * - Via argument parsing (secure_strdup)
 * - Via get_current_working_directory() for defaults
 */
void
cleanup (Config *config)
{
  if (config == NULL)
    {
      fprintf (stderr, "Warning: Attempted to cleanup NULL config\n");
      return;
    }

  // Free output_path if it was dynamically allocated
  // In this codebase, output_path is always either:
  // 1. NULL (not set)
  // 2. Dynamically allocated via secure_strdup (from arguments)
  // 3. Dynamically allocated via get_current_working_directory (default)
  if (config->output_path != NULL)
    {
      free (config->output_path);
      config->output_path = NULL;
    }

  // Note: config->url is const and should not be freed here
  // as it's either a string literal or managed elsewhere
}