#include "user_interaction.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Maximum input buffer size for reading user input
#define INPUT_BUFFER_SIZE 256

/**
 * Trim whitespace from both ends of a string in place.
 * @param str String to trim (modified in place)
 * @return Pointer to the trimmed string
 */
static char *
trim_whitespace (char *str)
{
  if (str == NULL)
    {
      return NULL;
    }

  // Trim leading whitespace
  char *start = str;
  while (*start && isspace ((unsigned char)*start))
    {
      start++;
    }

  // Trim trailing whitespace
  char *end = start + strlen (start) - 1;
  while (end > start && isspace ((unsigned char)*end))
    {
      end--;
    }
  *(end + 1) = '\0';

  // Return pointer to trimmed string
  return start;
}

/**
 * Validate format code input for security and correctness.
 * @param input Input string to validate
 * @return 0 if valid, -1 if invalid
 */
static int
validate_format_code (const char *input)
{
  if (input == NULL)
    {
      return -1;
    }

  size_t len = strlen (input);

  // Empty input is allowed (will use default)
  if (len == 0)
    {
      return 0;
    }

  // Check length constraint
  if (len >= FORMAT_CODE_LENGTH)
    {
      fprintf (stderr, "Error: Format code too long (max %d characters)\n",
               FORMAT_CODE_LENGTH - 1);
      return -1;
    }

  // Validate characters (allow alphanumeric, hyphens, underscores, dots)
  for (size_t i = 0; i < len; i++)
    {
      char c = input[i];
      if (!isalnum ((unsigned char)c) && c != '-' && c != '_' && c != '.')
        {
          fprintf (stderr,
                   "Error: Format code contains invalid character: '%c'\n", c);
          return -1;
        }
    }

  return 0;
}

/**
 * Prompt user for video format selection with comprehensive validation.
 * @return Allocated string containing format code, NULL on error
 */
char *
prompt_for_format (void)
{
  char input_buffer[INPUT_BUFFER_SIZE];

  printf ("Enter the format code (leave blank for best quality): ");
  fflush (stdout); // Ensure prompt is displayed

  if (fgets (input_buffer, sizeof (input_buffer), stdin) == NULL)
    {
      if (feof (stdin))
        {
          fprintf (stderr, "Error: End of input reached\n");
        }
      else
        {
          fprintf (stderr, "Error: Failed to read format code from input\n");
        }
      return NULL;
    }

  // Check if input was truncated (line too long)
  size_t input_len = strlen (input_buffer);
  if (input_len > 0 && input_buffer[input_len - 1] != '\n' && !feof (stdin))
    {
      // Clear the rest of the input line
      int c;
      while ((c = getchar ()) != '\n' && c != EOF)
        ;

      fprintf (stderr, "Error: Input too long (maximum %d characters)\n",
               INPUT_BUFFER_SIZE - 1);
      return NULL;
    }

  // Remove newline character
  input_buffer[strcspn (input_buffer, "\n")] = '\0';

  // Trim whitespace
  char *trimmed_input = trim_whitespace (input_buffer);

  // Validate the input
  if (validate_format_code (trimmed_input) != 0)
    {
      return NULL;
    }

  // Allocate and copy the validated input
  char *format_code = malloc (FORMAT_CODE_LENGTH * sizeof (char));
  if (format_code == NULL)
    {
      fprintf (stderr, "Error: Memory allocation failed for format code\n");
      return NULL;
    }

  // Safe copy with bounds checking
  if (snprintf (format_code, FORMAT_CODE_LENGTH, "%s", trimmed_input) < 0)
    {
      fprintf (stderr, "Error: Failed to copy format code\n");
      free (format_code);
      return NULL;
    }

  return format_code;
}