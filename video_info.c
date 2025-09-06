#include "video_info.h"
#include "command_execution.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Command constants for yt-dlp
#define YT_DLP_COMMAND "yt-dlp"
#define YT_DLP_JSON_FLAG "-j"

/**
 * Validate URL for basic security and format requirements.
 * @param url URL string to validate
 * @return 0 if valid, -1 if invalid
 */
static int
validate_url (const char *url)
{
  if (url == NULL)
    {
      fprintf (stderr, "Error: URL parameter is NULL\n");
      return -1;
    }

  size_t url_len = strlen (url);
  if (url_len == 0)
    {
      fprintf (stderr, "Error: URL is empty\n");
      return -1;
    }

  if (url_len >= MAX_URL_LENGTH)
    {
      fprintf (stderr, "Error: URL too long (max %d characters)\n",
               MAX_URL_LENGTH - 1);
      return -1;
    }

  // Basic URL validation - should start with http:// or https://
  if (url_len < 7
      || (strncmp (url, "http://", 7) != 0
          && strncmp (url, "https://", 8) != 0))
    {
      fprintf (stderr, "Error: URL must start with http:// or https://\n");
      return -1;
    }

  // Check for potentially dangerous characters in URL
  // Allow alphanumeric, and common URL characters
  for (size_t i = 0; i < url_len; i++)
    {
      char c = url[i];
      if (!isalnum ((unsigned char)c) && c != '/' && c != ':' && c != '.'
          && c != '-' && c != '_' && c != '?' && c != '=' && c != '&'
          && c != '%' && c != '+' && c != '#')
        {
          fprintf (stderr, "Error: URL contains invalid character: '%c'\n", c);
          return -1;
        }
    }

  return 0;
}

/**
 * Retrieve video information from yt-dlp with comprehensive validation.
 * @param url Video URL to fetch information for
 * @return Allocated JSON string containing video info, NULL on error
 */
char *
get_video_info (const char *url)
{
  if (validate_url (url) != 0)
    {
      return NULL;
    }

  printf ("Fetching video info...\n");

  // Build command arguments securely
  char *const argv[]
      = { YT_DLP_COMMAND, YT_DLP_JSON_FLAG,
          (char *)url, // Cast is safe since we validated the URL
          NULL };

  char *result = execute_command_with_output (YT_DLP_COMMAND, argv);
  if (result == NULL)
    {
      fprintf (stderr, "Error: Failed to execute yt-dlp command\n");
      return NULL;
    }

  return result;
}