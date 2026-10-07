# Input and Output Redirection Implementation in NexShell

This document provides a comprehensive technical overview of how **NexShell** implements standard input (`<`) and standard output (`>`) redirection using low-level POSIX system calls.

---

## 1. Redirection Concepts Overview

In Unix-like operating systems, processes interact with standard streams for reading inputs and writing outputs:

- **Output Redirection (`>`)**: Instructs the shell to redirect standard output (`STDOUT_FILENO`) away from the terminal screen and write it into a designated file. If the file does not exist, it is created. If the file already exists, its previous contents are truncated (overwritten).
- **Input Redirection (`<`)**: Instructs the shell to redirect standard input (`STDIN_FILENO`) so that a process reads data from a designated file rather than waiting for keyboard input from the terminal.
- **Dual Redirection (`<` and `>`)**: NexShell supports combining input and output redirection in a single command line (e.g., `cat < input.txt > output.txt` or `cat > output.txt < input.txt`).
- **Whitespace Flexibility (Tight Syntax)**: NexShell recognizes operators both with surrounding spaces (`ls > output.txt`) and without spaces (`ls>output.txt`, `cat<output.txt`).

---

## 2. Standard POSIX File Descriptors

Every process running on a Unix-like operating system maintains an indexed array of open file descriptions known as the **file descriptor table**. By POSIX convention, three standard streams are assigned fixed descriptor integers at process initialization:

| File Descriptor | POSIX Constant | Name | Default Binding |
| :---: | :---: | :---: | :---: |
| **`0`** | `STDIN_FILENO` | Standard Input | Terminal Keyboard |
| **`1`** | `STDOUT_FILENO` | Standard Output | Terminal Display Screen |
| **`2`** | `STDERR_FILENO` | Standard Error | Terminal Display Screen |

Redirection in NexShell operates by duplicating open file descriptors into slots `0` or `1` within the child process's file descriptor table, substituting standard keyboard or terminal endpoints with filesystem files before executing target binaries.

---

## 3. POSIX System Calls & Roles

NexShell relies on low-level POSIX system calls to perform robust, isolated file redirection:

| System Call | Header File | Function Signature | Role in NexShell Redirection |
| :--- | :--- | :--- | :--- |
| **`open()`** | `<fcntl.h>` | `int open(const char *path, int flags, mode_t mode)` | Opens or creates target files, returning a new file descriptor integer. |
| **`dup2()`** | `<unistd.h>` | `int dup2(int oldfd, int newfd)` | Atomically duplicates `oldfd` onto `newfd`. Closes `newfd` first if it was already open. |
| **`close()`** | `<unistd.h>` | `int close(int fd)` | Closes an active file descriptor, releasing table entries and kernel resources. |
| **`fork()`** | `<unistd.h>` | `pid_t fork(void)` | Clones the parent shell process to create a child process with an isolated file descriptor table. |
| **`execvp()`** | `<unistd.h>` | `int execvp(const char *file, char *const argv[])` | Replaces the child process memory space with the executable binary found via `PATH`. |
| **`waitpid()`** | `<sys/wait.h>` | `pid_t waitpid(pid_t pid, int *status, int options)` | Blocks the parent shell until the foreground child process terminates. |
| **`perror()`** | `<stdio.h>` | `void perror(const char *s)` | Outputs descriptive error messages to standard error (`stderr`). |

---

## 4. Deep Dive into Redirection Mechanics

### 4.1 How `open()` is Used

NexShell invokes `open()` inside the child process with specific access flags based on the redirection operator:

1. **Output Redirection (`>`)**:
   ```c
   int output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
   ```
   - `O_WRONLY`: Opens the file in write-only mode.
   - `O_CREAT`: Creates the file if it does not already exist.
   - `O_TRUNC`: Truncates existing file length to 0 bytes, ensuring overwriting rather than appending.
   - `0644`: Sets POSIX file permissions (Read/Write for owner, Read-only for group and others).

2. **Input Redirection (`<`)**:
   ```c
   int input_fd = open(input_file, O_RDONLY);
   ```
   - `O_RDONLY`: Opens the file in read-only mode.

If `open()` returns a negative integer (`< 0`), an error occurred (e.g., file not found, permission denied, or invalid path). NexShell reports the failure via `perror("open failed")` and calls `exit(1)` to terminate only the child process.

### 4.2 How `dup2()` is Used

The `dup2(int oldfd, int newfd)` system call duplicates an open file descriptor (`oldfd`) into a target slot (`newfd`):

- **Redirecting Standard Output**:
  ```c
  dup2(output_fd, STDOUT_FILENO); // STDOUT_FILENO == 1
  ```
  Any subsequent data written to standard output (descriptor `1`) by the executed program (e.g., via `printf`, `write`, or standard utilities like `ls`) is routed directly into `output_file`.

- **Redirecting Standard Input**:
  ```c
  dup2(input_fd, STDIN_FILENO); // STDIN_FILENO == 0
  ```
  Any subsequent read from standard input (descriptor `0`) by the executed program (e.g., `cat` or `grep`) reads directly from `input_file`.

### 4.3 Why `close()` is Used

Immediately after duplicating the descriptor with `dup2()`, the original descriptor is closed:

```c
close(output_fd); // or close(input_fd);
```

Closing the original descriptor is critical for several reasons:
1. **Descriptor Leak Prevention**: Operating systems impose limits on the maximum number of simultaneously open file descriptors per process (`RLIMIT_NOFILE`). Leaving unused file descriptors open exhausts table entries.
2. **Descriptor Hygiene**: Once slot `1` or slot `0` points to the target file, holding a duplicate descriptor open in `output_fd` or `input_fd` is redundant. Closing it prevents unintended descriptor inheritance by child programs.
3. **Buffer Flushing & Pipe Deadlocks**: In complex workflows and file systems, keeping redundant file descriptors open can prevent proper flush or EOF detection.

---

## 5. Execution Flow: Output Redirection (`ls > output.txt`)

When a user executes an output redirection command:
```bash
NexShell> ls > output.txt
```

NexShell processes execution through the following flow:

```
+-------------------------------------------------------------------------+
| 1. Read input "ls > output.txt", detect '>', split into:                |
|    - Command part: "ls"                                                 |
|    - Output file:  "output.txt"                                         |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| 2. Tokenize command part into args array: ["ls", NULL]                  |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| 3. Parent calls fork() to create child process                          |
+-------------------------------------------------------------------------+
            |                                           |
            | Child Process                             | Parent Process
            v                                           v
+---------------------------------------+  +------------------------------+
| 4. open("output.txt",                 |  | waitpid(pid, NULL, 0)        |
|         O_WRONLY|O_CREAT|O_TRUNC,     |  | (Blocks until child exits)   |
|         0644)                         |  +------------------------------+
+---------------------------------------+                 |
            |                                             |
            v                                             |
+---------------------------------------+                 |
| 5. dup2(output_fd, STDOUT_FILENO)     |                 |
|    (STDOUT descriptor 1 -> output_fd) |                 |
+---------------------------------------+                 |
            |                                             |
            v                                             |
+---------------------------------------+                 |
| 6. close(output_fd)                   |                 |
|    (Release redundant descriptor)     |                 |
+---------------------------------------+                 |
            |                                             |
            v                                             |
+---------------------------------------+                 |
| 7. execvp("ls", ["ls", NULL])         |                 |
|    (Output written to output.txt)     |                 |
+---------------------------------------+                 |
            |                                             |
            +--------------------[ Terminates ]-----------+
                                                          |
                                                          v
                                           +------------------------------+
                                           | Prompt redisplayed           |
                                           | NexShell>                    |
                                           +------------------------------+
```

### Step-by-Step Breakdown

1. **Detection & Parsing**: NexShell detects `>` using `strchr()`. It splits the command at `>`, trims whitespace from `"ls"` and `"output.txt"`, and sets `has_output_redirect = 1`.
2. **Argument Tokenization**: `parse_command()` tokenizes `"ls"` into `args[0] = "ls"`, `args[1] = NULL`. Notice that neither `>` nor `"output.txt"` are passed to `args`, ensuring `ls` receives clean arguments.
3. **Child Forking**: `pid = fork()` creates an isolated child process.
4. **File Opening**: Inside the child process (`pid == 0`), `open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644)` allocates `output_fd`.
5. **Stream Duplication**: `dup2(output_fd, STDOUT_FILENO)` overwrites descriptor `1` so stdout points to `output.txt`.
6. **Descriptor Cleanup**: `close(output_fd)` closes the original descriptor. Descriptor `1` remains pointed to `output.txt`.
7. **Execution**: `execvp(args[0], args)` executes `/bin/ls`. All output generated by `ls` is written into `output.txt`.
8. **Parent Synchronization**: The parent process waits for the child using `waitpid(pid, NULL, 0)` before printing the next prompt.

---

## 6. Execution Flow: Input Redirection (`cat < output.txt`)

When a user executes an input redirection command:
```bash
NexShell> cat < output.txt
```

NexShell processes execution through the following flow:

```
+-------------------------------------------------------------------------+
| 1. Read input "cat < output.txt", detect '<', split into:               |
|    - Command part: "cat"                                                |
|    - Input file:   "output.txt"                                         |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| 2. Tokenize command part into args array: ["cat", NULL]                 |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| 3. Parent calls fork() to create child process                          |
+-------------------------------------------------------------------------+
            |                                           |
            | Child Process                             | Parent Process
            v                                           v
+---------------------------------------+  +------------------------------+
| 4. open("output.txt", O_RDONLY)       |  | waitpid(pid, NULL, 0)        |
|    (Opens file for reading)           |  | (Blocks until child exits)   |
+---------------------------------------+  +------------------------------+
            |                                             |
            v                                             |
+---------------------------------------+                 |
| 5. dup2(input_fd, STDIN_FILENO)       |                 |
|    (STDIN descriptor 0 -> input_fd)   |                 |
+---------------------------------------+                 |
            |                                             |
            v                                             |
+---------------------------------------+                 |
| 6. close(input_fd)                    |                 |
|    (Release redundant descriptor)     |                 |
+---------------------------------------+                 |
            |                                             |
            v                                             |
+---------------------------------------+                 |
| 7. execvp("cat", ["cat", NULL])       |                 |
|    (cat reads data from output.txt)   |                 |
+---------------------------------------+                 |
            |                                             |
            +--------------------[ Terminates ]-----------+
                                                          |
                                                          v
                                           +------------------------------+
                                           | Prompt redisplayed           |
                                           | NexShell>                    |
                                           +------------------------------+
```

### Step-by-Step Breakdown

1. **Detection & Parsing**: NexShell detects `<` using `strchr()`. It splits the string at `<`, trims `"cat"` and `"output.txt"`, and sets `has_input_redirect = 1`.
2. **Argument Tokenization**: `parse_command()` tokenizes `"cat"` into `args[0] = "cat"`, `args[1] = NULL`.
3. **Child Forking**: `fork()` spawns the child process.
4. **File Opening**: Inside the child process (`pid == 0`), `open("output.txt", O_RDONLY)` opens the file in read-only mode.
5. **Stream Duplication**: `dup2(input_fd, STDIN_FILENO)` points descriptor `0` (`stdin`) to `output.txt`.
6. **Descriptor Cleanup**: `close(input_fd)` closes the extra descriptor.
7. **Execution**: `execvp("cat", args)` replaces the process with `/bin/cat`. Instead of waiting for keyboard input, `cat` reads directly from `output.txt` and prints to standard output (terminal).
8. **Parent Synchronization**: The parent process waits via `waitpid(pid, NULL, 0)`.

---

## 7. Dual Redirection and Tight Syntax

### 7.1 Simultaneous Dual Redirection
NexShell seamlessly handles both operators when present on a single line:
```bash
NexShell> cat < input.txt > output.txt
NexShell> cat > output.txt < input.txt
```
NexShell inspects the relative positions of `<` and `>` in the input string:
- If `<` occurs before `>`, the string is parsed as: `[command] < [input_file] > [output_file]`.
- If `>` occurs before `<`, the string is parsed as: `[command] > [output_file] < [input_file]`.

In the child process, both redirections are applied in order: `dup2(input_fd, STDIN_FILENO)` followed by `dup2(output_fd, STDOUT_FILENO)`.

### 7.2 Operator Parsing Without Spaces (Tight Syntax)
Because NexShell locates operators using `strchr()` and splits string pointers directly rather than relying on whitespace tokenization:
- `ls>output.txt` splits into command `"ls"` and output file `"output.txt"`.
- `cat<output.txt` splits into command `"cat"` and input file `"output.txt"`.

Both tight syntax and space-separated syntax work without separate parsing passes.

---

## 8. Error Handling & Shell Stability

NexShell implements strict error handling at every stage of redirection:

1. **Missing Filename or Command**:
   - If a user inputs `ls >` or `cat <`, NexShell detects an empty filename string and reports `Error: Missing output filename.` or `Error: Missing input filename.`.
   - If a user inputs `> output.txt` or `< input.txt`, NexShell reports `Error: Missing command before '>'.` or `Error: Missing command before '<'.`.
   - In both cases, the shell skips process creation and directly returns to the prompt.

2. **File Open Errors (Isolated in Child Process)**:
   - If a user specifies a non-existent file for input (`cat < missing.txt`) or an invalid directory path for output (`ls > /invalid/path/file.txt`), `open()` fails and returns `-1`.
   - Because `open()` is called inside the child process, the child prints the system error via `perror("open failed")` and calls `exit(1)`.
   - The parent shell remains active, safely reaps the terminated child via `waitpid()`, and immediately returns to the `NexShell> ` prompt without crashing.
