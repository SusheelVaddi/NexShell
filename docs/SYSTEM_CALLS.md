# NexShell System Call & POSIX Interface Reference

This document provides a comprehensive technical reference for every POSIX system call and standard C library function utilized within **NexShell**.

---

## 1. Standard File Descriptors Overview

In Unix-like operating systems, every process starts with three default open file descriptors:

| Descriptor Constant | Integer Value | Description | Default Stream Target |
| :--- | :--- | :--- | :--- |
| **`STDIN_FILENO`** | `0` | Standard Input | Keyboard / Pipe Read End / Input File |
| **`STDOUT_FILENO`** | `1` | Standard Output | Terminal Display / Pipe Write End / Output File |
| **`STDERR_FILENO`** | `2` | Standard Error | Terminal Display (Unbuffered diagnostic output) |

NexShell manipulates these three descriptors using `dup2()` to achieve transparent I/O redirection and piping.

---

## 2. System Calls & Low-Level Interfaces

### 2.1 `fork()`
- **Header**: `<unistd.h>`, `<sys/types.h>`
- **Prototype**: `pid_t fork(void);`
- **Purpose**: Creates a new process by duplicating the calling process (parent). The new process (child) has its own independent address space, descriptor table, and execution context.
- **Parameters**: None.
- **Return Value**:
  - `0`: Returned inside the child process.
  - `> 0`: Process ID (PID) of the child, returned inside the parent process.
  - `-1`: Fork failed (system out of memory or process limit reached).
- **How NexShell Uses It**: Spawns worker processes for all external commands, pipelines, and background tasks.
- **Example**:
  ```c
  pid_t pid = fork();
  if (pid < 0) {
      perror("fork failed");
  } else if (pid == 0) {
      // Child process logic
  } else {
      // Parent process logic
  }
  ```

---

### 2.2 `execvp()`
- **Header**: `<unistd.h>`
- **Prototype**: `int execvp(const char *file, char *const argv[]);`
- **Purpose**: Replaces the current process image with a new process image specified by `file`. Searches directories listed in the `PATH` environment variable if `file` does not contain a slash `/`.
- **Parameters**:
  - `file`: The executable name or binary path.
  - `argv`: A `NULL`-terminated array of argument strings where `argv[0]` is the program name.
- **Return Value**: Only returns `-1` if execution fails. On success, it does not return because the calling program is completely overwritten.
- **How NexShell Uses It**: Executes external binaries (`ls`, `pwd`, `mkdir`, `grep`, `cat`, etc.) in child processes.
- **Example**:
  ```c
  char *args[] = {"ls", "-l", NULL};
  execvp(args[0], args);
  perror("execvp failed");
  exit(1);
  ```

---

### 2.3 `waitpid()`
- **Header**: `<sys/wait.h>`, `<sys/types.h>`
- **Prototype**: `pid_t waitpid(pid_t pid, int *status, int options);`
- **Purpose**: Suspends the calling process until a specific child process changes state, or reaps terminated child processes without blocking when `WNOHANG` is supplied.
- **Parameters**:
  - `pid`: Child PID to wait for. Passing `-1` waits for any child process.
  - `status`: Pointer to store child exit status (or `NULL` if status is unneeded).
  - `options`: Flag modifiers (`0` for blocking wait, `WNOHANG` for non-blocking return).
- **Return Value**: Child PID whose state changed, `0` if `WNOHANG` used and no child has exited, or `-1` on error.
- **How NexShell Uses It**:
  1. Synchronous foreground execution: `waitpid(pid, NULL, 0)`.
  2. Non-blocking zombie cleanup: `while (waitpid(-1, NULL, WNOHANG) > 0)`.

---

### 2.4 `pipe()`
- **Header**: `<unistd.h>`
- **Prototype**: `int pipe(int pipefd[2]);`
- **Purpose**: Allocates a unidirectional data channel in kernel memory with two file descriptors.
- **Parameters**:
  - `pipefd[0]`: Read end of the pipe.
  - `pipefd[1]`: Write end of the pipe.
- **Return Value**: `0` on success, `-1` on error.
- **How NexShell Uses It**: Sets up the communication channel for command pipelines (`cmd1 | cmd2`).
- **Example**:
  ```c
  int pipe_fd[2];
  if (pipe(pipe_fd) < 0) {
      perror("pipe failed");
  }
  ```

---

### 2.5 `dup2()`
- **Header**: `<unistd.h>`
- **Prototype**: `int dup2(int oldfd, int newfd);`
- **Purpose**: Duplicates an open file descriptor `oldfd` onto `newfd`. If `newfd` is currently open, it is silently closed first.
- **Parameters**:
  - `oldfd`: Existing source file descriptor.
  - `newfd`: Target file descriptor to overwrite (`STDIN_FILENO` or `STDOUT_FILENO`).
- **Return Value**: `newfd` on success, `-1` on error.
- **How NexShell Uses It**: Binds opened files and pipe descriptors to `STDIN_FILENO` (0) and `STDOUT_FILENO` (1).
- **Example**:
  ```c
  // Redirect standard output to a file
  int fd = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
  dup2(fd, STDOUT_FILENO);
  close(fd);
  ```

---

### 2.6 `open()` and `close()`
- **Header**: `<fcntl.h>`, `<unistd.h>`
- **Prototypes**:
  - `int open(const char *pathname, int flags, mode_t mode);`
  - `int close(int fd);`
- **Purpose**: `open()` connects a file to a new file descriptor; `close()` releases the descriptor back to the operating system.
- **Key Flags Used in NexShell**:
  - `O_RDONLY`: Open for read-only access (used for input redirection `<`).
  - `O_WRONLY`: Open for write-only access (used for output redirection `>`).
  - `O_CREAT`: Create file if it does not exist.
  - `O_TRUNC`: Truncate file length to 0 if it already exists.
  - `0644` Mode: Read/write permission for owner, read-only for group/others.
- **How NexShell Uses It**: Opens redirection target files and closes file descriptors in parent and child processes to prevent descriptor leaks.

---

### 2.7 `chdir()`
- **Header**: `<unistd.h>`
- **Prototype**: `int chdir(const char *path);`
- **Purpose**: Updates the current working directory of the calling process to `path`.
- **Return Value**: `0` on success, `-1` on error.
- **How NexShell Uses It**: Implements the built-in `cd` command inside the parent shell process.
- **Why It Must Run in Parent**: If `chdir()` were executed inside a child process, only the child's working directory would change, leaving the interactive shell in the original directory upon child exit.

---

### 2.8 `perror()`
- **Header**: `<stdio.h>`
- **Prototype**: `void perror(const char *s);`
- **Purpose**: Prints an error message to `stderr` combining the user-supplied string `s` with the text description of the current `errno` value.
- **How NexShell Uses It**: Provides descriptive system error messages for failed system calls (`perror("cd failed")`, `perror("open failed")`, `perror("dup2 failed")`, `perror("execvp failed")`).
