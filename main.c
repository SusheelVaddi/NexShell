#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

// Define maximum buffer sizes for user input and arguments
#define MAX_INPUT_SIZE 1024
#define MAX_ARGS 64

// Helper function to trim leading and trailing whitespace from a string
static char *trim_whitespace(char *str) {
    while (*str == ' ' || *str == '\t') {
        str++;
    }
    if (*str == '\0') {
        return str;
    }
    char *end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t')) {
        *end = '\0';
        end--;
    }
    return str;
}

// Helper function to tokenize a command string into an array of arguments
static int parse_command(char *cmd_str, char **args, int max_args) {
    int count = 0;
    char *token = strtok(cmd_str, " \t");
    while (token != NULL && count < max_args - 1) {
        args[count++] = token;
        token = strtok(NULL, " \t");
    }
    args[count] = NULL;
    return count;
}

// Structure to store parsed redirection components and flags
typedef struct {
    char *cmd_part;
    char *input_file;
    char *output_file;
    int has_input_redirect;
    int has_output_redirect;
} RedirectionInfo;

// Helper function to parse and validate redirection syntax
static int parse_redirection(char *input_str, RedirectionInfo *redir) {
    // Count occurrences of redirection operators to detect repeated or multiple usage
    int count_out = 0;
    int count_in = 0;
    for (int i = 0; input_str[i] != '\0'; i++) {
        if (input_str[i] == '>') {
            count_out++;
        } else if (input_str[i] == '<') {
            count_in++;
        }
    }

    // No redirection requested
    if (count_out == 0 && count_in == 0) {
        return 0;
    }

    // Validate against repeated or multiple redirection operators
    if (count_out > 1) {
        printf("Error: Multiple or repeated '>' redirection operators.\n");
        return -1;
    }
    if (count_in > 1) {
        printf("Error: Multiple or repeated '<' redirection operators.\n");
        return -1;
    }

    // Initialize RedirectionInfo fields
    redir->cmd_part = NULL;
    redir->input_file = NULL;
    redir->output_file = NULL;
    redir->has_input_redirect = 0;
    redir->has_output_redirect = 0;

    char *out_redirect_ptr = strchr(input_str, '>');
    char *in_redirect_ptr = strchr(input_str, '<');

    if (out_redirect_ptr != NULL && in_redirect_ptr != NULL) {
        // Both operators present; inspect relative ordering
        if (in_redirect_ptr < out_redirect_ptr) {
            // Format: cmd < input_file > output_file
            *in_redirect_ptr = '\0';
            *out_redirect_ptr = '\0';
            redir->cmd_part = trim_whitespace(input_str);
            redir->input_file = trim_whitespace(in_redirect_ptr + 1);
            redir->output_file = trim_whitespace(out_redirect_ptr + 1);
        } else {
            // Format: cmd > output_file < input_file
            *out_redirect_ptr = '\0';
            *in_redirect_ptr = '\0';
            redir->cmd_part = trim_whitespace(input_str);
            redir->output_file = trim_whitespace(out_redirect_ptr + 1);
            redir->input_file = trim_whitespace(in_redirect_ptr + 1);
        }
        redir->has_input_redirect = 1;
        redir->has_output_redirect = 1;
    } else if (out_redirect_ptr != NULL) {
        // Only '>' present
        *out_redirect_ptr = '\0';
        redir->cmd_part = trim_whitespace(input_str);
        redir->output_file = trim_whitespace(out_redirect_ptr + 1);
        redir->has_output_redirect = 1;
    } else {
        // Only '<' present
        *in_redirect_ptr = '\0';
        redir->cmd_part = trim_whitespace(input_str);
        redir->input_file = trim_whitespace(in_redirect_ptr + 1);
        redir->has_input_redirect = 1;
    }

    // Validate extracted filenames
    if (redir->has_output_redirect && strlen(redir->output_file) == 0) {
        printf("Error: Missing output filename.\n");
        return -1;
    }
    if (redir->has_input_redirect && strlen(redir->input_file) == 0) {
        printf("Error: Missing input filename.\n");
        return -1;
    }

    // Validate extracted command part
    if (strlen(redir->cmd_part) == 0) {
        if (redir->has_output_redirect && !redir->has_input_redirect) {
            printf("Error: Missing command before '>'.\n");
        } else if (redir->has_input_redirect && !redir->has_output_redirect) {
            printf("Error: Missing command before '<'.\n");
        } else {
            printf("Error: Missing command before redirection.\n");
        }
        return -1;
    }

    return 1;
}

// Helper function to configure file descriptors and execute command in child process
static void execute_child_redirection(const RedirectionInfo *redir, char **args) {
    // Set up input redirection if requested
    if (redir->has_input_redirect) {
        // Open source file in read-only mode
        int input_fd = open(redir->input_file, O_RDONLY);
        if (input_fd < 0) {
            perror("open failed");
            exit(1);
        }

        // Duplicate input_fd to STDIN_FILENO (descriptor 0)
        if (dup2(input_fd, STDIN_FILENO) < 0) {
            perror("dup2 failed");
            close(input_fd); // Close descriptor on error before exit
            exit(1);
        }

        // Close original descriptor after dup2 to prevent file descriptor leaks
        close(input_fd);
    }

    // Set up output redirection if requested
    if (redir->has_output_redirect) {
        // Open destination file: write-only, create if missing, truncate if existing
        int output_fd = open(redir->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (output_fd < 0) {
            perror("open failed");
            exit(1);
        }

        // Duplicate output_fd to STDOUT_FILENO (descriptor 1)
        if (dup2(output_fd, STDOUT_FILENO) < 0) {
            perror("dup2 failed");
            close(output_fd); // Close descriptor on error before exit
            exit(1);
        }

        // Close original descriptor after dup2 to prevent file descriptor leaks
        close(output_fd);
    }

    // Replace child image with target program
    execvp(args[0], args);

    // If execvp returns, command execution failed
    perror("execvp failed");
    exit(1);
}

int main(void) {
    char input[MAX_INPUT_SIZE];

    // Infinite loop to keep the shell running continuously
    while (1) {
        // Non-blocking check to reap any completed background processes (avoids zombies)
        while (waitpid(-1, NULL, WNOHANG) > 0) {
            // Reap zombie background processes
        }

        // Display the NexShell prompt
        printf("NexShell> ");
        fflush(stdout); // Flush stdout to guarantee the prompt prints before waiting for input

        // Read a line of input from standard input (keyboard)
        if (fgets(input, sizeof(input), stdin) == NULL) {
            // Handle Ctrl+D / EOF or input error gracefully
            printf("\nExiting NexShell...\n");
            break;
        }

        // Remove the trailing newline character standard in fgets input
        input[strcspn(input, "\n")] = '\0';

        // Check for background execution operator '&' at the end of input
        int is_background = 0;
        char *bg_ptr = strrchr(input, '&');
        if (bg_ptr != NULL) {
            // Ensure '&' is at the end of the command (ignoring trailing whitespace)
            char *trailing = bg_ptr + 1;
            while (*trailing == ' ' || *trailing == '\t') {
                trailing++;
            }
            if (*trailing == '\0') {
                is_background = 1;
                *bg_ptr = '\0'; // Remove '&' from input
            }
        }

        // Trim leading and trailing whitespace from input
        char *trimmed_input = trim_whitespace(input);

        // If the user pressed Enter or input is empty, prompt again
        if (strlen(trimmed_input) == 0) {
            if (is_background) {
                printf("Error: Missing command before '&'.\n");
            }
            continue;
        }

        // Check for built-in exit command
        if (strcmp(trimmed_input, "exit") == 0) {
            printf("Exiting NexShell...\n");
            break;
        }

        // Check for built-in cd command (must execute in parent process)
        if (strncmp(trimmed_input, "cd", 2) == 0 && (trimmed_input[2] == ' ' || trimmed_input[2] == '\0' || trimmed_input[2] == '\t')) {
            char *dir = trimmed_input + 2;

            // Skip leading spaces after "cd"
            while (*dir == ' ' || *dir == '\t') {
                dir++;
            }

            if (*dir == '\0') {
                dir = getenv("HOME");
            }

            if (dir != NULL && chdir(dir) != 0) {
                perror("cd failed");
            }
            continue;
        }

        // Check for pipe operator '|'
        char *pipe_ptr = strchr(trimmed_input, '|');
        if (pipe_ptr != NULL) {
            // Split input into left command and right command
            *pipe_ptr = '\0';
            char *left_cmd_part = trim_whitespace(trimmed_input);
            char *right_cmd_part = trim_whitespace(pipe_ptr + 1);

            // Validate right command part
            if (strlen(right_cmd_part) == 0) {
                printf("Error: Missing command after '|'.\n");
                continue;
            }

            // Validate left command part
            if (strlen(left_cmd_part) == 0) {
                printf("Error: Missing command before '|'.\n");
                continue;
            }

            // Tokenize left command part into arguments for execvp
            char *left_args[MAX_ARGS];
            int left_count = parse_command(left_cmd_part, left_args, MAX_ARGS);
            if (left_count == 0) {
                printf("Error: Invalid command before '|'.\n");
                continue;
            }

            // Tokenize right command part into arguments for execvp
            char *right_args[MAX_ARGS];
            int right_count = parse_command(right_cmd_part, right_args, MAX_ARGS);
            if (right_count == 0) {
                printf("Error: Invalid command after '|'.\n");
                continue;
            }

            // Create pipe file descriptors (pipe_fd[0] is read end, pipe_fd[1] is write end)
            int pipe_fd[2];
            if (pipe(pipe_fd) < 0) {
                perror("pipe failed");
                continue;
            }

            // Fork first child process for the LEFT command
            pid_t pid1 = fork();
            if (pid1 < 0) {
                perror("fork failed");
                close(pipe_fd[0]);
                close(pipe_fd[1]);
                continue;
            } else if (pid1 == 0) {
                // Left child: redirect STDOUT to pipe write end
                if (dup2(pipe_fd[1], STDOUT_FILENO) < 0) {
                    perror("dup2 failed");
                    close(pipe_fd[0]);
                    close(pipe_fd[1]);
                    exit(1);
                }

                // Close unused pipe file descriptors in left child
                close(pipe_fd[0]);
                close(pipe_fd[1]);

                // Execute left command
                execvp(left_args[0], left_args);
                perror("execvp failed");
                exit(1);
            }

            // Fork second child process for the RIGHT command
            pid_t pid2 = fork();
            if (pid2 < 0) {
                perror("fork failed");
                close(pipe_fd[0]);
                close(pipe_fd[1]);
                waitpid(pid1, NULL, 0); // Wait for first child if second fork fails
                continue;
            } else if (pid2 == 0) {
                // Right child: redirect STDIN to pipe read end
                if (dup2(pipe_fd[0], STDIN_FILENO) < 0) {
                    perror("dup2 failed");
                    close(pipe_fd[0]);
                    close(pipe_fd[1]);
                    exit(1);
                }

                // Close unused pipe file descriptors in right child
                close(pipe_fd[0]);
                close(pipe_fd[1]);

                // Execute right command
                execvp(right_args[0], right_args);
                perror("execvp failed");
                exit(1);
            }

            // Parent process: close both ends of the pipe
            close(pipe_fd[0]);
            close(pipe_fd[1]);

            if (is_background) {
                printf("[Background process started: PIDs %d, %d]\n", pid1, pid2);
            } else {
                // Wait for both children in foreground execution
                waitpid(pid1, NULL, 0);
                waitpid(pid2, NULL, 0);
            }
            continue;
        }

        // Check for redirection operators ('>' and '<')
        RedirectionInfo redir;
        int redir_status = parse_redirection(trimmed_input, &redir);

        if (redir_status < 0) {
            // Syntax or validation error occurred; skip execution and prompt again
            continue;
        } else if (redir_status > 0) {
            // Tokenize command part into arguments for execvp
            char *args[MAX_ARGS];
            int arg_count = parse_command(redir.cmd_part, args, MAX_ARGS);

            if (arg_count == 0) {
                printf("Error: Invalid command before redirection.\n");
                continue;
            }

            // Fork a child process to execute the command with redirection
            pid_t pid = fork();

            if (pid < 0) {
                // Fork failed in parent process
                perror("fork failed");
                continue;
            } else if (pid == 0) {
                // Child process: configure file descriptors and execute command
                execute_child_redirection(&redir, args);
            } else {
                // Parent process: wait for foreground child or report background PID
                if (is_background) {
                    printf("[Background process started: PID %d]\n", pid);
                } else {
                    waitpid(pid, NULL, 0);
                }
            }
            continue;
        }

        // General external command execution (handles ls, pwd, mkdir, cat, sleep, etc.)
        char *args[MAX_ARGS];
        int arg_count = parse_command(trimmed_input, args, MAX_ARGS);

        if (arg_count > 0) {
            pid_t pid = fork();

            if (pid < 0) {
                // Fork failed
                perror("fork failed");
            } else if (pid == 0) {
                // Child process: execute command using execvp
                execvp(args[0], args);

                // If execvp returns, command execution failed
                perror("execvp failed");
                exit(1);
            } else {
                // Parent process
                if (is_background) {
                    printf("[Background process started: PID %d]\n", pid);
                } else {
                    // Foreground process: wait for child process to complete
                    waitpid(pid, NULL, 0);
                }
            }
        }
    }

    return 0;
}
