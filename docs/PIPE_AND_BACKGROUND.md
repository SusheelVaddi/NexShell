# Pipe and Background Execution Implementation in NexShell

This document provides a detailed overview of how **NexShell** implements command piping (`|`) and background process execution (`&`) using standard POSIX system calls.

---

## 1. System Calls Overview

NexShell relies on low-level POSIX system calls to manage processes and inter-process communication (IPC):

| System Call / Flag | Header File | Role & Mechanism in NexShell |
| :--- | :--- | :--- |
| **`pipe(int pipe_fd[2])`** | `<unistd.h>` | Allocates a unidirectional data channel in kernel memory. `pipe_fd[0]` is opened for reading and `pipe_fd[1]` is opened for writing. |
| **`fork()`** | `<unistd.h>` | Clones the calling process to create a child process with an isolated memory space and duplicated file descriptors. Returns `0` inside the child process and the child's PID inside the parent process. |
| **`dup2(int oldfd, int newfd)`** | `<unistd.h>` | Duplicates file descriptor `oldfd` onto `newfd`. Used to redirect standard streams (`STDIN_FILENO` / `STDOUT_FILENO`). |
| **`execvp(const char *file, char *const argv[])`** | `<unistd.h>` | Replaces the current child process image with a new executable program binary searched from the system `PATH`. |
| **`waitpid(pid_t pid, int *status, int options)`** | `<sys/wait.h>` | Blocks or polls until a specific child process changes execution state (e.g., terminates). |
| **`WNOHANG`** | `<sys/wait.h>` | Option flag passed to `waitpid()`. Instructs `waitpid` to return immediately without blocking if no child process has exited. |

---

## 2. Command Pipeline Execution Flow (`ls | sort`)

When a user executes a piped command such as:
```bash
NexShell> ls | sort
```
NexShell executes the following sequence:

```
+-------------------------------------------------------------------+
| 1. Read input "ls | sort", detect '|', split into "ls" and "sort"  |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
| 2. Call pipe(pipe_fd): creates pipe_fd[0] (read) & pipe_fd[1] (write) |
+-------------------------------------------------------------------+
                                  |
                                  v
+---------------------------------+---------------------------------+
|                                                                   |
v                                                                   v
+----------------------------------+  +----------------------------------+
| 3. Fork Left Child (pid1)        |  | 4. Fork Right Child (pid2)       |
| - dup2(pipe_fd[1], STDOUT_FILENO)|  | - dup2(pipe_fd[0], STDIN_FILENO) |
| - close(pipe_fd[0]) & [1]        |  | - close(pipe_fd[0]) & [1]        |
| - execvp("ls", ["ls", NULL])     |  | - execvp("sort", ["sort", NULL]) |
+----------------------------------+  +----------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
| 5. Parent Process:                                                |
| - Close pipe_fd[0] and pipe_fd[1]                                 |
| - waitpid(pid1, NULL, 0) followed by waitpid(pid2, NULL, 0)       |
+-------------------------------------------------------------------+
```

### Step-by-Step Breakdown

1. **Input Parsing & Tokenization**:
   - `main.c` scans the sanitized input string for `|` using `strchr()`.
   - The string is split into `left_cmd_part` (`"ls"`) and `right_cmd_part` (`"sort"`).
   - `parse_command()` tokenizes both parts into argument arrays: `left_args = ["ls", NULL]` and `right_args = ["sort", NULL]`.

2. **Pipe Creation**:
   - `pipe(pipe_fd)` allocates two file descriptors:
     - `pipe_fd[0]`: Read end of the pipe.
     - `pipe_fd[1]`: Write end of the pipe.

3. **Left Process Creation (`ls`)**:
   - `pid1 = fork()` spawns the first child process.
   - Inside the left child (`pid1 == 0`):
     - `dup2(pipe_fd[1], STDOUT_FILENO)` connects the child's standard output to the pipe write end.
     - `close(pipe_fd[0])` and `close(pipe_fd[1])` release unused descriptors.
     - `execvp("ls", left_args)` replaces the child image with `ls`. All output from `ls` flows directly into the pipe.

4. **Right Process Creation (`sort`)**:
   - `pid2 = fork()` spawns the second child process.
   - Inside the right child (`pid2 == 0`):
     - `dup2(pipe_fd[0], STDIN_FILENO)` connects the child's standard input to the pipe read end.
     - `close(pipe_fd[0])` and `close(pipe_fd[1])` release unused descriptors.
     - `execvp("sort", right_args)` replaces the child image with `sort`. It reads its input from the pipe write end and prints sorted results to standard output.

5. **Parent Cleanup & Process Synchronization**:
   - The parent process closes both `pipe_fd[0]` and `pipe_fd[1]`. (Closing the write end in the parent is critical so that `sort` receives End-of-File (EOF) when `ls` terminates).
   - For foreground execution (`is_background == 0`), the parent calls `waitpid(pid1, NULL, 0)` and `waitpid(pid2, NULL, 0)`, pausing until both children exit before redisplaying the prompt.

---

## 3. Background Execution Flow (`sleep 10 &`)

When a user executes a background command such as:
```bash
NexShell> sleep 10 &
```
NexShell processes asynchronous execution through the following flow:

```
+-------------------------------------------------------------------+
| 1. Read input "sleep 10 &", detect trailing '&'                   |
| - Set is_background = 1                                           |
| - Remove '&' -> trimmed command becomes "sleep 10"                |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
| 2. Tokenize arguments: args = ["sleep", "10", NULL]              |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
| 3. Fork child process: pid = fork()                               |
+-------------------------------------------------------------------+
                 |                                 |
                 v                                 v
+---------------------------------+  +---------------------------------+
| Child Process (pid == 0):       |  | Parent Process (pid > 0):       |
| execvp("sleep", ["sleep","10"]) |  | - Print "[Background process..."|
|                                 |  | - Skip waitpid(pid, NULL, 0)    |
|                                 |  | - Return immediately to prompt  |
+---------------------------------+  +---------------------------------+
                                                   |
                                                   v
+-------------------------------------------------------------------+
| 4. Non-blocking Zombie Reaping (Next Prompt Cycle):               |
| while (waitpid(-1, NULL, WNOHANG) > 0) { /* reap zombies */ }     |
+-------------------------------------------------------------------+
```

### Step-by-Step Breakdown

1. **Background Symbol Detection**:
   - NexShell checks for `&` at the end of the input using `strrchr()`.
   - If found, `is_background` flag is set to `1`, and `&` is stripped from the command string.

2. **Child Execution**:
   - `pid = fork()` creates a child process.
   - The child calls `execvp("sleep", args)` to run `sleep 10` asynchronously.

3. **Parent Non-Blocking Return**:
   - Because `is_background == 1`, the parent shell prints:
     `[Background process started: PID <pid>]`
   - The parent bypasses blocking `waitpid()` calls and immediately loops back to display `NexShell> `. The user can continue issuing new commands while `sleep 10` runs concurrently.

4. **Zombie Process Prevention with `WNOHANG`**:
   - When background commands complete, they enter a "zombie" state until their exit status is collected by the parent.
   - At the beginning of every prompt loop iteration in `main.c`:
     ```c
     while (waitpid(-1, NULL, WNOHANG) > 0) {
         // Reap zombie background processes
     }
     ```
   - Passing `-1` tells `waitpid` to check any child process.
   - Passing `WNOHANG` ensures the check is non-blocking (returns immediately with `0` if no child has finished), preventing memory leaks without delaying shell prompt responsiveness.
