#include "terminal_ui.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// Column widths for format display
#define COL_INDEX_WIDTH 4
#define COL_FORMAT_ID_WIDTH 6
#define COL_RESOLUTION_WIDTH 12
#define COL_EXTENSION_WIDTH 6
#define COL_FILESIZE_WIDTH 10
#define COL_QUALITY_WIDTH 12

/**
 * Get format information from JSON object safely.
 * @param format JSON format object
 * @param format_id Output for format ID
 * @param resolution Output for resolution
 * @param ext Output for extension
 * @param filesize Output for file size
 * @return 0 on success, -1 on error
 */
static int
get_format_info (json_t *format, const char **format_id,
                 const char **resolution, const char **ext,
                 json_int_t *filesize)
{
  if (format == NULL || !json_is_object (format))
    {
      return -1;
    }

  json_t *id_obj = json_object_get (format, "format_id");
  *format_id = (id_obj && json_is_string (id_obj)) ? json_string_value (id_obj)
                                                   : "N/A";

  json_t *res_obj = json_object_get (format, "resolution");
  *resolution = (res_obj && json_is_string (res_obj))
                    ? json_string_value (res_obj)
                    : "N/A";

  json_t *ext_obj = json_object_get (format, "ext");
  *ext = (ext_obj && json_is_string (ext_obj)) ? json_string_value (ext_obj)
                                               : "N/A";

  json_t *size_obj = json_object_get (format, "filesize");
  *filesize = (size_obj && json_is_integer (size_obj))
                  ? json_integer_value (size_obj)
                  : 0;

  return 0;
}

/**
 * Determine quality label for format.
 * @param resolution Resolution string
 * @param ext Extension string
 * @return Quality label string
 */
static const char *
get_quality_label (const char *resolution, const char *ext)
{
  if (resolution == NULL || strcmp (resolution, "N/A") == 0)
    {
      if (ext
          && (strcmp (ext, "m4a") == 0 || strcmp (ext, "webm") == 0
              || strcmp (ext, "opus") == 0))
        {
          return "Audio Only";
        }
      return "";
    }

  // Extract height from resolution (e.g., "1920x1080" -> 1080)
  const char *x_pos = strchr (resolution, 'x');
  if (x_pos)
    {
      int height = atoi (x_pos + 1);
      if (height >= 2160)
        return "4K UHD";
      if (height >= 1440)
        return "2K QHD";
      if (height >= 1080)
        return "Full HD";
      if (height >= 720)
        return "HD";
      if (height >= 480)
        return "SD";
    }

  return "";
}

/**
 * Draw format table header.
 * @param win Window to draw in
 * @param y Starting Y position
 * @param colors_supported Whether colors are supported
 */
static void
draw_format_header (WINDOW *win, int y, bool colors_supported)
{
  if (colors_supported)
    {
      wattron (win, COLOR_PAIR (COLOR_PAIR_BORDER) | A_BOLD);
    }

  mvwprintw (win, y, 1, "%-*s %-*s %-*s %-*s %-*s %-*s", COL_INDEX_WIDTH, "#",
             COL_FORMAT_ID_WIDTH, "Format", COL_RESOLUTION_WIDTH, "Resolution",
             COL_EXTENSION_WIDTH, "Type", COL_FILESIZE_WIDTH, "Size",
             COL_QUALITY_WIDTH, "Quality");

  // Draw separator line
  mvwhline (win, y + 1, 1, ACS_HLINE, getmaxx (win) - 2);

  if (colors_supported)
    {
      wattroff (win, COLOR_PAIR (COLOR_PAIR_BORDER) | A_BOLD);
    }
}

/**
 * Draw a single format entry.
 * @param win Window to draw in
 * @param y Y position
 * @param index Format index
 * @param format JSON format object
 * @param selected Whether this format is selected
 * @param colors_supported Whether colors are supported
 */
static void
draw_format_entry (WINDOW *win, int y, int index, json_t *format,
                   bool selected, bool colors_supported)
{
  const char *format_id, *resolution, *ext;
  json_int_t filesize;

  if (get_format_info (format, &format_id, &resolution, &ext, &filesize) != 0)
    {
      return;
    }

  // Format file size
  char size_str[32];
  if (filesize > 0)
    {
      ui_format_bytes (filesize, size_str, sizeof (size_str));
    }
  else
    {
      strcpy (size_str, "N/A");
    }

  // Get quality label
  const char *quality = get_quality_label (resolution, ext);

  // Apply selection highlighting
  if (selected && colors_supported)
    {
      wattron (win, COLOR_PAIR (COLOR_PAIR_SELECTED) | A_BOLD);
    }

  // Clear the line first
  mvwhline (win, y, 1, ' ', getmaxx (win) - 2);

  // Draw format entry
  mvwprintw (win, y, 1, "%-*d %-*s %-*s %-*s %-*s", COL_INDEX_WIDTH, index + 1,
             COL_FORMAT_ID_WIDTH, format_id, COL_RESOLUTION_WIDTH, resolution,
             COL_EXTENSION_WIDTH, ext, COL_FILESIZE_WIDTH, size_str);

  // Draw quality label with special color if not selected
  if (!selected && colors_supported && strlen (quality) > 0)
    {
      if (strstr (quality, "4K") || strstr (quality, "Full HD"))
        {
          wattron (win, COLOR_PAIR (COLOR_PAIR_SUCCESS));
        }
      else if (strstr (quality, "Audio"))
        {
          wattron (win, COLOR_PAIR (COLOR_PAIR_WARNING));
        }
    }

  mvwprintw (win, y,
             1 + COL_INDEX_WIDTH + COL_FORMAT_ID_WIDTH + COL_RESOLUTION_WIDTH
                 + COL_EXTENSION_WIDTH + COL_FILESIZE_WIDTH + 5,
             "%-*s", COL_QUALITY_WIDTH, quality);

  // Reset attributes
  if (selected && colors_supported)
    {
      wattroff (win, COLOR_PAIR (COLOR_PAIR_SELECTED) | A_BOLD);
    }
  else if (!selected && colors_supported && strlen (quality) > 0)
    {
      if (strstr (quality, "4K") || strstr (quality, "Full HD"))
        {
          wattroff (win, COLOR_PAIR (COLOR_PAIR_SUCCESS));
        }
      else if (strstr (quality, "Audio"))
        {
          wattroff (win, COLOR_PAIR (COLOR_PAIR_WARNING));
        }
    }
}

/**
 * Display formats in the content window.
 * @param state UI state structure
 * @param formats JSON array of formats
 * @param list_state Format list state for scrolling
 * @return 0 on success, -1 on error
 */
int
ui_display_formats (UIState *state, json_t *formats,
                    FormatListState *list_state)
{
  if (state == NULL || formats == NULL || list_state == NULL
      || !state->ncurses_available)
    {
      return -1;
    }

  ui_lock (state);

  WINDOW *win = state->content_window;
  werase (win);

  // Initialize list state only if not already initialized
  if (list_state->formats == NULL)
    {
      list_state->formats = formats;
      list_state->total_formats = json_array_size (formats);
      list_state->selected_index = 0;
      list_state->visible_start = 0;
    }

  // Calculate visible lines (leave room for header and borders)
  int content_height = getmaxy (win);
  list_state->visible_lines = content_height - 4; // Header + borders

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
  mvwprintw (win, 0, 2, " Available Formats (%d total) ",
             list_state->total_formats);

  // Draw header
  draw_format_header (win, 2, state->colors_supported);

  // Draw visible formats
  int y = 4; // Start after header
  for (int i = 0;
       i < list_state->visible_lines
       && (list_state->visible_start + i) < list_state->total_formats;
       i++)
    {
      int format_index = list_state->visible_start + i;
      json_t *format = json_array_get (formats, format_index);

      bool selected = (format_index == list_state->selected_index);
      draw_format_entry (win, y + i, format_index, format, selected,
                         state->colors_supported);
    }

  // Draw scroll indicators if needed
  if (list_state->visible_start > 0)
    {
      mvwprintw (win, 3, getmaxx (win) - 3, "↑");
    }
  if (list_state->visible_start + list_state->visible_lines
      < list_state->total_formats)
    {
      mvwprintw (win, content_height - 2, getmaxx (win) - 3, "↓");
    }

  wrefresh (win);
  update_panels ();
  doupdate ();
  ui_unlock (state);

  return 0;
}

/**
 * Update format list display after navigation.
 * @param state UI state structure
 * @param list_state Format list state
 */
static void
update_format_display (UIState *state, FormatListState *list_state)
{
  // Ensure selected index is visible
  if (list_state->selected_index < list_state->visible_start)
    {
      list_state->visible_start = list_state->selected_index;
    }
  else if (list_state->selected_index
           >= list_state->visible_start + list_state->visible_lines)
    {
      list_state->visible_start
          = list_state->selected_index - list_state->visible_lines + 1;
    }

  // Redraw the format list
  ui_display_formats (state, list_state->formats, list_state);
}

/**
 * Handle special format selection shortcuts.
 * @param ch Character input
 * @param list_state Format list state
 * @return 1 if handled, 0 otherwise
 */
static int
handle_format_shortcut (int ch, FormatListState *list_state)
{
  switch (ch)
    {
    case 'b':
    case 'B':
      // Select best quality (usually first)
      list_state->selected_index = 0;
      return 1;

    case 'w':
    case 'W':
      // Select worst quality (usually last)
      list_state->selected_index = list_state->total_formats - 1;
      return 1;

    case 'a':
    case 'A':
      // Select first audio-only format
      for (int i = 0; i < list_state->total_formats; i++)
        {
          json_t *format = json_array_get (list_state->formats, i);
          json_t *res_obj = json_object_get (format, "resolution");
          const char *resolution = (res_obj && json_is_string (res_obj))
                                       ? json_string_value (res_obj)
                                       : "N/A";

          if (strcmp (resolution, "N/A") == 0 || strstr (resolution, "audio"))
            {
              list_state->selected_index = i;
              return 1;
            }
        }
      break;

    default:
      // Check for number keys (1-9)
      if (ch >= '1' && ch <= '9')
        {
          int index = ch - '1';
          if (index < list_state->total_formats)
            {
              list_state->selected_index = index;
              return 1;
            }
        }
      break;
    }

  return 0;
}

/**
 * Interactive format selection with keyboard navigation.
 * @param state UI state structure
 * @param list_state Format list state
 * @return Selected format code (allocated), NULL on cancel
 */
char *
ui_select_format_interactive (UIState *state, FormatListState *list_state)
{
  if (state == NULL || list_state == NULL || !state->ncurses_available)
    {
      return NULL;
    }

  // Show initial status
  ui_show_status (state, "Select a format to download");

  // Enable blocking input for selection
  nodelay (stdscr, FALSE);
  timeout (-1); // Blocking input for selection

  // Make sure the content window is current and refreshed
  touchwin (state->content_window);
  wrefresh (state->content_window);
  update_panels ();
  doupdate ();

  // Ensure content panel is on top
  top_panel (state->content_panel);

  char *selected_format = NULL;
  bool selecting = true;

  while (selecting && !state->shutdown_pending)
    {
      // Handle resize if needed
      ui_handle_resize (state);

      // Get user input
      int ch = wgetch (state->content_window);

      switch (ch)
        {
        case KEY_UP:
          if (list_state->selected_index > 0)
            {
              list_state->selected_index--;
              update_format_display (state, list_state);
            }
          break;

        case KEY_DOWN:
          if (list_state->selected_index < list_state->total_formats - 1)
            {
              list_state->selected_index++;
              update_format_display (state, list_state);
            }
          break;

        case KEY_PPAGE: // Page Up
          list_state->selected_index -= list_state->visible_lines;
          if (list_state->selected_index < 0)
            {
              list_state->selected_index = 0;
            }
          update_format_display (state, list_state);
          break;

        case KEY_NPAGE: // Page Down
          list_state->selected_index += list_state->visible_lines;
          if (list_state->selected_index >= list_state->total_formats)
            {
              list_state->selected_index = list_state->total_formats - 1;
            }
          update_format_display (state, list_state);
          break;

        case KEY_HOME:
          list_state->selected_index = 0;
          update_format_display (state, list_state);
          break;

        case KEY_END:
          list_state->selected_index = list_state->total_formats - 1;
          update_format_display (state, list_state);
          break;

        case '\n':
        case '\r':
        case KEY_ENTER:
          // Get selected format code
          {
            json_t *format = json_array_get (list_state->formats,
                                             list_state->selected_index);
            json_t *id_obj = json_object_get (format, "format_id");
            if (id_obj && json_is_string (id_obj))
              {
                const char *format_id = json_string_value (id_obj);
                selected_format = malloc (strlen (format_id) + 1);
                if (selected_format)
                  {
                    strcpy (selected_format, format_id);
                  }
              }
          }
          selecting = false;
          break;

        case 27: // ESC - could be escape key or start of escape sequence
          {
            // Set a short timeout to check for escape sequence
            wtimeout (state->content_window,
                      100); // Increased to 100ms timeout
            int next_ch = wgetch (state->content_window);

            if (next_ch == ERR)
              {
                // No following character, it's a real ESC key press
                selecting = false;
              }
            else if (next_ch == '[')
              {
                // It's an escape sequence, get the third character
                int third_ch = wgetch (state->content_window);

                // Handle arrow keys sent as escape sequences
                switch (third_ch)
                  {
                  case 'A': // Up arrow
                    if (list_state->selected_index > 0)
                      {
                        list_state->selected_index--;
                        update_format_display (state, list_state);
                      }
                    break;
                  case 'B': // Down arrow
                    if (list_state->selected_index
                        < list_state->total_formats - 1)
                      {
                        list_state->selected_index++;
                        update_format_display (state, list_state);
                      }
                    break;
                  case 'C': // Right arrow - could map to page down
                    break;
                  case 'D': // Left arrow - could map to page up
                    break;
                  case '5': // Page Up (ESC[5~)
                    if (wgetch (state->content_window) == '~')
                      {
                        list_state->selected_index
                            -= list_state->visible_lines;
                        if (list_state->selected_index < 0)
                          {
                            list_state->selected_index = 0;
                          }
                        update_format_display (state, list_state);
                      }
                    break;
                  case '6': // Page Down (ESC[6~)
                    if (wgetch (state->content_window) == '~')
                      {
                        list_state->selected_index
                            += list_state->visible_lines;
                        if (list_state->selected_index
                            >= list_state->total_formats)
                          {
                            list_state->selected_index
                                = list_state->total_formats - 1;
                          }
                        update_format_display (state, list_state);
                      }
                    break;
                  }
              }

            // Restore blocking timeout
            wtimeout (state->content_window, -1);
          }
          break;

        case 'q':
        case 'Q':
          selecting = false;
          break;

        default:
          // Check for shortcuts
          if (handle_format_shortcut (ch, list_state))
            {
              update_format_display (state, list_state);
            }
          break;
        }
    }

  // Restore non-blocking input
  nodelay (stdscr, TRUE);
  timeout (UI_UPDATE_INTERVAL_MS); // Restore original timeout

  // Hide content panel
  hide_panel (state->content_panel);

  return selected_format;
}
