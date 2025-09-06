#include "format_parsing.h"

#include <jansson.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// JSON field names as constants
#define JSON_FIELD_FORMATS "formats"
#define JSON_FIELD_FORMAT_ID "format_id"
#define JSON_FIELD_RESOLUTION "resolution"
#define JSON_FIELD_EXTENSION "ext"
#define JSON_FIELD_FILESIZE "filesize"

// Display format constants
#define FORMAT_ID_WIDTH 5
#define RESOLUTION_WIDTH 10
#define EXTENSION_WIDTH 4
#define FILESIZE_WIDTH 8

// Maximum reasonable JSON string length to prevent DoS
#define MAX_JSON_LENGTH (1024 * 1024) // 1MB

/**
 * Safely retrieve a string value from JSON object with NULL checking.
 * @param obj JSON object
 * @param key Field name
 * @return String value or NULL if not found/invalid
 */
static const char *
safe_json_string_value (const json_t *obj, const char *key)
{
  if (obj == NULL || key == NULL || !json_is_object (obj))
    {
      return NULL;
    }

  json_t *value = json_object_get (obj, key);
  if (value == NULL || !json_is_string (value))
    {
      return NULL;
    }

  return json_string_value (value);
}

/**
 * Safely retrieve an integer value from JSON object with bounds checking.
 * @param obj JSON object
 * @param key Field name
 * @param result Pointer to store result
 * @return 0 on success, -1 on error
 */
static int
safe_json_integer_value (const json_t *obj, const char *key,
                         json_int_t *result)
{
  if (obj == NULL || key == NULL || result == NULL || !json_is_object (obj))
    {
      return -1;
    }

  json_t *value = json_object_get (obj, key);
  if (value == NULL || !json_is_integer (value))
    {
      return -1;
    }

  *result = json_integer_value (value);

  // Check for reasonable file size bounds (prevent negative or extremely large
  // values)
  if (*result < 0 || *result > INT64_MAX / 2)
    {
      fprintf (stderr, "Warning: Invalid file size value: %lld\n",
               (long long)*result);
      return -1;
    }

  return 0;
}

/**
 * Parse JSON string and extract formats array with comprehensive validation.
 * @param json_str JSON string to parse
 * @return JSON array of formats, NULL on error
 */
json_t *
parse_formats (const char *json_str)
{
  if (json_str == NULL)
    {
      fprintf (stderr, "Error: JSON string parameter is NULL\n");
      return NULL;
    }

  size_t json_len = strlen (json_str);
  if (json_len == 0)
    {
      fprintf (stderr, "Error: JSON string is empty\n");
      return NULL;
    }

  if (json_len > MAX_JSON_LENGTH)
    {
      fprintf (stderr, "Error: JSON string too large (%zu bytes, max %d)\n",
               json_len, MAX_JSON_LENGTH);
      return NULL;
    }

  json_error_t error;
  json_t *root = json_loads (json_str, 0, &error);
  if (root == NULL)
    {
      fprintf (stderr, "Error: JSON parsing failed on line %d: %s\n",
               error.line, error.text);
      return NULL;
    }

  if (!json_is_object (root))
    {
      fprintf (stderr, "Error: Root JSON element is not an object\n");
      json_decref (root);
      return NULL;
    }

  json_t *formats = json_object_get (root, JSON_FIELD_FORMATS);
  if (formats == NULL)
    {
      fprintf (stderr, "Error: '%s' field not found in JSON data\n",
               JSON_FIELD_FORMATS);
      json_decref (root);
      return NULL;
    }

  if (!json_is_array (formats))
    {
      fprintf (stderr, "Error: '%s' is not an array in JSON data\n",
               JSON_FIELD_FORMATS);
      json_decref (root);
      return NULL;
    }

  // Validate that formats array is not empty
  size_t array_size = json_array_size (formats);
  if (array_size == 0)
    {
      fprintf (stderr, "Error: Formats array is empty\n");
      json_decref (root);
      return NULL;
    }

  // Increment reference count to keep formats alive after root is decref'd
  json_incref (formats);
  json_decref (root);
  return formats;
}

/**
 * Display available formats in a formatted table with safe field access.
 * @param formats JSON array of format objects
 */
void
display_formats (const json_t *formats)
{
  if (formats == NULL || !json_is_array (formats))
    {
      fprintf (stderr, "Error: Invalid formats parameter\n");
      return;
    }

  printf ("Available formats:\n");

  size_t index;
  json_t *format;
  json_array_foreach (formats, index, format)
  {
    if (format == NULL || !json_is_object (format))
      {
        fprintf (stderr,
                 "Warning: Skipping invalid format entry at index %zu\n",
                 index);
        continue;
      }

    const char *format_id
        = safe_json_string_value (format, JSON_FIELD_FORMAT_ID);
    const char *resolution
        = safe_json_string_value (format, JSON_FIELD_RESOLUTION);
    const char *ext = safe_json_string_value (format, JSON_FIELD_EXTENSION);

    json_int_t filesize = 0;
    int filesize_valid
        = safe_json_integer_value (format, JSON_FIELD_FILESIZE, &filesize);

    printf (
        "Format code: %-*s Resolution: %-*s Extension: %-*s Filesize: %*s\n",
        FORMAT_ID_WIDTH, format_id ? format_id : "N/A", RESOLUTION_WIDTH,
        resolution ? resolution : "N/A", EXTENSION_WIDTH, ext ? ext : "N/A",
        FILESIZE_WIDTH, filesize_valid == 0 ? "bytes" : "N/A");

    if (filesize_valid == 0)
      {
        printf ("%*lld ", FILESIZE_WIDTH, (long long)filesize);
      }
    printf ("\n");
  }
}