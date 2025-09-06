#ifndef DOWNLOAD_HELPERS_H
#define DOWNLOAD_HELPERS_H

#include "ytdl.h"

// clang-format off
char **build_download_command_args(const char *format_code, const char *output_path, const char *url);
void free_command_args(char **args);
int download_video(const Config *config, const char *format_code);
// clang-format on

#endif