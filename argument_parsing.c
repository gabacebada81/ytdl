#include "argument_parsing.h"
#include "directory_management.h"
#include "help_display.h"

#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Secure string duplication with overflow protection and length validation.
 * @param s Source string to duplicate
 * @param max_len Maximum allowed length (including null terminator)
 * @return Duplicated string or NULL on failure
 */
static char *
secure_strdup (const char *s, size_t max_len)
{
  if (s == NULL)
    {
      return NULL;
    }

  size_t len = strlen (s);
  if (len >= max_len)
    {
      fprintf (stderr,
               "Error: String length exceeds maximum allowed (%zu >= %zu)\n",
               len, max_len);
      return NULL;
    }

  // Check for potential overflow: len + 1 > SIZE_MAX
  if (len > SIZE_MAX - 1)
    {
      fprintf (stderr,
               "Error: String too long, would cause size_t overflow\n");
      return NULL;
    }

  char *new_str = malloc (len + 1);
  if (new_str == NULL)
    {
      fprintf (stderr, "Error: Memory allocation failed\n");
      return NULL;
    }

  memcpy (new_str, s, len + 1);
  return new_str;
}

/**
 * Parse command line arguments and populate configuration structure.
 * @param argc Argument count
 * @param argv Argument vector
 * @param config Configuration structure to populate
 * @return 0 on success, EXIT_FAILURE on error
 */
int
parse_arguments (int argc, char *argv[], Config *config)
{
  if (config == NULL || argv == NULL)
    {
      fprintf (stderr, "Error: Invalid arguments to parse_arguments\n");
      return EXIT_FAILURE;
    }

  struct option long_options[] = { { "help", no_argument, 0, 'h' },
                                   { "output", required_argument, 0, 'o' },
                                   { 0, 0, 0, 0 } };

  int opt;
  opterr = 0; // Suppress getopt error messages for cleaner output

  while ((opt = getopt_long (argc, argv, "ho:", long_options, NULL)) != -1)
    {
      switch (opt)
        {
        case 'h':
          display_help ();
          exit (EXIT_SUCCESS);
        case 'o':
          config->output_path = secure_strdup (optarg, MAX_PATH_LENGTH);
          if (config->output_path == NULL)
            {
              return EXIT_FAILURE;
            }
          break;
        case '?':
          fprintf (stderr, "Error: Unknown option or missing argument\n");
          display_help ();
          return EXIT_FAILURE;
        default:
          fprintf (stderr, "Error: Unexpected option parsing error\n");
          return EXIT_FAILURE;
        }
    }

  // Validate URL argument
  if (optind < argc)
    {
      size_t url_len = strlen (argv[optind]);
      if (url_len >= MAX_URL_LENGTH)
        {
          fprintf (stderr, "Error: URL too long (max %d characters)\n",
                   MAX_URL_LENGTH - 1);
          return EXIT_FAILURE;
        }
      config->url = argv[optind];
    }
  else
    {
      fprintf (stderr, "Error: URL is required\n");
      display_help ();
      return EXIT_FAILURE;
    }

  return EXIT_SUCCESS;
}

/**
 * Initialize the output path, creating default or specified directory.
 * @param config Configuration structure
 * @return 0 on success, EXIT_FAILURE on error
 */
int
initialize_output_path (Config *config)
{
  if (config == NULL)
    {
      fprintf (stderr, "Error: Invalid config parameter\n");
      return EXIT_FAILURE;
    }

  if (config->output_path == NULL)
    {
      char *allocated_path = get_current_working_directory ();
      if (allocated_path == NULL)
        {
          fprintf (stderr, "Error: Failed to get current working directory\n");
          return EXIT_FAILURE;
        }
      config->output_path = allocated_path;
    }
  else
    {
      if (create_directory_if_not_exists (config->output_path) == -1)
        {
          fprintf (stderr, "Error: Failed to create output directory: %s\n",
                   config->output_path);
          return EXIT_FAILURE;
        }
    }

  return EXIT_SUCCESS;
}
