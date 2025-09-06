#ifndef YTDL_H
#define YTDL_H

#include <errno.h>
#include <getopt.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_URL_LENGTH 2048
#define MAX_PATH_LENGTH 4096
#define BUFFER_SIZE 1024
#define FORMAT_CODE_LENGTH 20
#define DIRECTORY_PERMISSIONS (S_IRWXU)

enum
{
  READ_END,
  WRITE_END
};

typedef struct
{
  const char *url;
  char *output_path;
} Config;

#endif
