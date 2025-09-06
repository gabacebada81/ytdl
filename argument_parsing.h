#ifndef ARGUMENT_PARSING_H
#define ARGUMENT_PARSING_H

#include "ytdl.h"

// clang-format off
int parse_arguments(int argc, char *argv[], Config *config);
int initialize_output_path(Config *config);
// clang-format on

#endif