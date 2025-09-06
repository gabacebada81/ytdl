#include "command_execution.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/**
 * Safely close a file descriptor with error checking.
 * @param fd File descriptor to close
 * @return 0 on success, -1 on failure
 */
static int
safe_close (int fd)
{
  if (fd >= 0 && close (fd) == -1)
    {
      perror ("close");
      return -1;
    }
  return 0;
}

/**
 * Validate child process exit status and handle errors.
 * @param status Process status from waitpid
 * @param output Output buffer to free on error (can be NULL)
 * @return exit status on success, -1 on error (frees output if provided)
 */
static int
validate_child_status (int status, char **output)
{
  if (WIFEXITED (status))
    {
      int exit_status = WEXITSTATUS (status);
      if (exit_status != 0)
        {
          fprintf (stderr, "Error: Command exited with status %d\n",
                   exit_status);
          if (output && *output)
            {
              free (*output);
              *output = NULL;
            }
          return -1;
        }
      return exit_status;
    }
  else
    {
      fprintf (stderr, "Error: Child process did not exit normally\n");
      if (output && *output)
        {
          free (*output);
          *output = NULL;
        }
      return -1;
    }
}

/**
 * Fork a new process with error handling.
 * @return pid of child process, -1 on error
 */
pid_t
fork_process (void)
{
  pid_t pid = fork ();
  if (pid == -1)
    {
      perror ("fork");
    }
  return pid;
}

/**
 * Create a pipe with bounds checking and error handling.
 * @param pipefd Array to store pipe file descriptors
 * @return 0 on success, -1 on error
 */
int
setup_pipes (int pipefd[2])
{
  if (pipefd == NULL)
    {
      fprintf (stderr, "Error: Invalid pipefd parameter\n");
      return -1;
    }

  if (pipe (pipefd) == -1)
    {
      perror ("pipe");
      return -1;
    }
  return 0;
}

/**
 * Redirect stdout to pipe with error handling.
 * @param pipefd Write end of pipe
 * @return 0 on success, -1 on error
 */
int
redirect_stdout (int pipefd)
{
  if (dup2 (pipefd, STDOUT_FILENO) == -1)
    {
      perror ("dup2");
      return -1;
    }
  return 0;
}

/**
 * Read data from pipe with secure buffer management and overflow protection.
 * @param pipefd Read end of pipe
 * @return Allocated string containing pipe output, NULL on error
 */
char *
read_from_pipe (int pipefd)
{
  char buffer[BUFFER_SIZE];
  ssize_t bytes_read;
  size_t output_size = 0;
  size_t output_capacity = BUFFER_SIZE; // Start with reasonable capacity
  char *output = malloc (output_capacity);

  if (output == NULL)
    {
      perror ("malloc");
      return NULL;
    }
  output[0] = '\0';

  while ((bytes_read = read (pipefd, buffer, BUFFER_SIZE - 1)) > 0)
    {
      buffer[bytes_read] = '\0';

      // Check for potential size overflow
      if (output_size > SIZE_MAX - bytes_read - 1)
        {
          fprintf (stderr, "Error: Output size would overflow\n");
          free (output);
          return NULL;
        }

      size_t new_size = output_size + bytes_read + 1;

      // Grow buffer if needed (use exponential growth for efficiency)
      if (new_size > output_capacity)
        {
          size_t new_capacity = output_capacity * 2;
          if (new_capacity < new_size)
            {
              new_capacity = new_size;
            }

          // Prevent capacity overflow
          if (new_capacity > SIZE_MAX / 2)
            {
              new_capacity = SIZE_MAX / 2;
              if (new_capacity < new_size)
                {
                  fprintf (stderr, "Error: Required buffer size too large\n");
                  free (output);
                  return NULL;
                }
            }

          char *new_output = realloc (output, new_capacity);
          if (new_output == NULL)
            {
              perror ("realloc");
              free (output);
              return NULL;
            }
          output = new_output;
          output_capacity = new_capacity;
        }

      memcpy (output + output_size, buffer, bytes_read + 1);
      output_size += bytes_read;
    }

  if (bytes_read == -1)
    {
      perror ("read");
      free (output);
      return NULL;
    }

  return output;
}

/**
 * Execute command and capture its output.
 * @param command Command to execute
 * @param argv Argument vector (NULL-terminated)
 * @return Allocated string containing command output, NULL on error
 */
char *
execute_command_with_output (const char *command, char *const argv[])
{
  if (command == NULL || argv == NULL)
    {
      fprintf (stderr,
               "Error: Invalid parameters to execute_command_with_output\n");
      return NULL;
    }

  int pipefd[2];
  if (setup_pipes (pipefd) == -1)
    {
      return NULL;
    }

  pid_t pid = fork_process ();
  if (pid == -1)
    {
      safe_close (pipefd[READ_END]);
      safe_close (pipefd[WRITE_END]);
      return NULL;
    }
  else if (pid == 0)
    {
      // Child process
      safe_close (pipefd[READ_END]);

      if (redirect_stdout (pipefd[WRITE_END]) == -1)
        {
          safe_close (pipefd[WRITE_END]);
          exit (EXIT_FAILURE);
        }
      safe_close (pipefd[WRITE_END]);

      execvp (command, argv);
      perror ("execvp");
      exit (EXIT_FAILURE);
    }
  else
    {
      // Parent process
      safe_close (pipefd[WRITE_END]);

      char *output = read_from_pipe (pipefd[READ_END]);
      safe_close (pipefd[READ_END]);

      int status;
      if (waitpid (pid, &status, 0) == -1)
        {
          perror ("waitpid");
          free (output);
          return NULL;
        }

      if (validate_child_status (status, &output) == -1)
        {
          return NULL;
        }

      return output;
    }
}

/**
 * Execute command without capturing output.
 * @param command Command to execute
 * @param argv Argument vector (NULL-terminated)
 * @return Exit status of command, -1 on error
 */
int
execute_command_without_output (const char *command, char *const argv[])
{
  if (command == NULL || argv == NULL)
    {
      fprintf (
          stderr,
          "Error: Invalid parameters to execute_command_without_output\n");
      return -1;
    }

  pid_t pid = fork_process ();
  if (pid == -1)
    {
      return -1;
    }
  else if (pid == 0)
    {
      // Child process
      execvp (command, argv);
      perror ("execvp");
      exit (EXIT_FAILURE);
    }
  else
    {
      // Parent process
      int status;
      if (waitpid (pid, &status, 0) == -1)
        {
          perror ("waitpid");
          return -1;
        }

      return validate_child_status (status, NULL);
    }
}