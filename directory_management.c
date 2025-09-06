#include "directory_management.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * Validate a path for security and correctness.
 * @param path Path to validate
 * @return 0 if valid, -1 if invalid
 */
static int
validate_path (const char *path)
{
  if (path == NULL)
    {
      fprintf (stderr, "Error: Path parameter is NULL\n");
      return -1;
    }

  size_t path_len = strlen (path);
  if (path_len == 0)
    {
      fprintf (stderr, "Error: Path is empty\n");
      return -1;
    }

  if (path_len >= MAX_PATH_LENGTH)
    {
      fprintf (stderr, "Error: Path length exceeds maximum (%zu >= %d)\n",
               path_len, MAX_PATH_LENGTH);
      return -1;
    }

  // Check for directory traversal attempts
  if (strstr (path, "..") != NULL)
    {
      fprintf (stderr, "Error: Path contains directory traversal sequence\n");
      return -1;
    }

  // Check for absolute path requirement (optional but recommended)
  if (path[0] != '/')
    {
      fprintf (stderr, "Error: Path must be absolute\n");
      return -1;
    }

  return 0;
}

/**
 * Create a directory if it doesn't exist with proper validation and error
 * handling.
 * @param path Absolute path to the directory to create
 * @return 0 on success, -1 on error
 */
int
create_directory_if_not_exists (const char *path)
{
  if (validate_path (path) == -1)
    {
      return -1;
    }

  struct stat st = { 0 };
  if (stat (path, &st) == -1)
    {
      // Directory doesn't exist, try to create it
      if (errno == ENOENT)
        {
          if (mkdir (path, DIRECTORY_PERMISSIONS) == -1)
            {
              fprintf (stderr, "Error: Failed to create directory '%s': %s\n",
                       path, strerror (errno));
              return -1;
            }
        }
      else
        {
          // Other stat error
          fprintf (stderr, "Error: Failed to stat directory '%s': %s\n", path,
                   strerror (errno));
          return -1;
        }
    }
  else
    {
      // Path exists, check if it's actually a directory
      if (!S_ISDIR (st.st_mode))
        {
          fprintf (stderr, "Error: Path '%s' exists but is not a directory\n",
                   path);
          return -1;
        }
    }

  return 0;
}

/**
 * Get the current working directory with enhanced error handling.
 * @return Allocated string containing current directory path, NULL on error
 */
char *
get_current_working_directory (void)
{
  // First, try to get the required buffer size
  size_t required_size = (size_t)pathconf (".", _PC_PATH_MAX);
  if (required_size == (size_t)-1)
    {
      // Fallback to a reasonable default if pathconf fails
      required_size = MAX_PATH_LENGTH;
    }

  char *current_dir = malloc (required_size);
  if (current_dir == NULL)
    {
      fprintf (stderr,
               "Error: Memory allocation failed for current directory\n");
      return NULL;
    }

  if (getcwd (current_dir, required_size) == NULL)
    {
      fprintf (stderr, "Error: Failed to get current working directory: %s\n",
               strerror (errno));
      free (current_dir);
      return NULL;
    }

  // Verify the path is valid
  if (validate_path (current_dir) == -1)
    {
      free (current_dir);
      return NULL;
    }

  return current_dir;
}