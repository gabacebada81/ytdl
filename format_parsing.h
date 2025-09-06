#ifndef FORMAT_PARSING_H
#define FORMAT_PARSING_H

#include "ytdl.h"

// clang-format off
json_t *parse_formats(const char *json_str);
void display_formats(const json_t *formats);
// clang-format on

#endif