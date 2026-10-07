# NexShell Complete Architecture & Systems Design

This document provides an in-depth architectural breakdown of **NexShell**, explaining its execution lifecycle, data flow, memory structures, process models, and system call interactions.

---

## 1. High-Level Architectural Flow

```
                      +-----------------------------+
                      |         User Input          |
                      +-----------------------------+
                                     |
                                     v
                      +-----------------------------+
                      |    Non-blocking Reaping     |
                      | waitpid(-1, NULL, WNOHANG)  |
                      +-----------------------------+
                                     |
                                     v
                      +-----------------------------+
                      |   Prompt & Read (fgets)     |
                      +-----------------------------+
                                     |
                                     v
                      +-----------------------------+
                      |    Sanitization & Trim      |
                      |  Strip newline & whitespace |
                      +-----------------------------+
                                     |
                                     v
                      +-----------------------------+
                      |  Operator Identification    |
                      +-----------------------------+
                         /         |          \
                        /          |           \
                       v           v            v
             +--------------+ +-----------+ +---------------+
             |   Built-in   | |  Pipe (|) | | Redirection   |
             |   cd / exit  | |  Channel  | |   (> / <)     |
             +--------------+ +-----------+ +---------------+
                    |              |                |
                    |              v                v
                    |       +-------------+  +--------------+
                    |       | pipe() FDs  |  | open() FDs   |
                    |       +-------------+  +--------------+
                    |              |                |
                    v              v                v
             +--------------+ +-----------+ +---------------+
             |   Parent     | |  fork() 2 | | fork() child  |
             |  Execution   | |  children | | + dup2() FDs  |
             +--------------+ +-----------+ +---------------+
                                   |                |
                                   +-------+--------+
                                           |
                                           v
                                   +----------------+
                                   |    execvp()    |
                                   +----------------+
                                           |
                                           v
                                   +----------------+
                                   | Process Sync   |
                                   | waitpid() vs & |
                                   +----------------+
```

---

## 2. Component Architecture Breakdown

### 2.1 The REPL Loop (Read-Eval-Print Loop)
NexShell runs inside a continuous loop in `main()`:
1. **Reap Phase**: Checks for dead background child processes using `waitpid(-1, NULL, WNOHANG)`.
2. **Prompt Phase**: Flushes `stdout` and outputs `NexShell> `.
3. **Read Phase**: Reads up to `MAX_INPUT_SIZE` (1024 bytes) from `stdin` via `fgets()`. Gracefully terminates on EOF (`Ctrl+D`).
4. **Eval Phase**: Dispatches input to parsers and executes the command.
5. **Print Phase**: Program output prints to stdout; errors print to stderr via `perror()`.

---

### 2.2 Input Sanitization & Whitespace Trimming
Raw input contains a trailing newline `\n` from `fgets()`. 
- `input[strcspn(input, "\n")] = '\0'` strips the newline.
- `trim_whitespace()` scans from the beginning to skip spaces/tabs, then scans backwards from the string end replacing whitespace with `\0`.

---

### 2.3 Background Execution Detection (`&`)
NexShell inspects the end of the input for the ampersand operator `&`:
- Finds last occurrence with `strrchr(input, '&')`.
- Verifies that only spaces or tabs follow `&`.
- Sets `is_background = 1` and replaces `&` with `\0`.
- If the remaining command is empty, reports `Error: Missing command before '&'.`.

---

### 2.4 Built-in Command Handling (`cd`, `exit`)
Built-ins must modify the state of the parent shell rather than running inside an isolated child process:
- **`exit`**: Breaks out of the `while (1)` loop, prints termination banner, and returns 0.
- **`cd [dir]`**: 
  - Tokenizes destination directory string.
  - If no directory is supplied, resolves user home directory using `getenv("HOME")`.
  - Invokes `chdir(dir)`.
  - On error, calls `perror("cd failed")`.

---

### 2.5 Command Piping Architecture (`|`)
When `|` is detected:
```
+---------------+                              +---------------+
|  Left Child   |                              |  Right Child  |
|  (e.g., ls)   |                              | (e.g., grep)  |
|               |                              |               |
|  STDOUT (1)   | ---> [ pipe_fd[1] (Write) ]  |               |
|               |              |               |               |
+---------------+              v               +---------------+
                     [ Pipe Kernel Buffer ]            ^
                               |                       |
                               +---> [ pipe_fd[0] (Read) ] === STDIN (0)
```

1. Splits input into `left_cmd_part` and `right_cmd_part`.
2. Validates non-empty strings on both sides.
3. Tokenizes both subcommands into argument arrays.
4. Allocates pipe file descriptors via `pipe(pipe_fd)`.
5. Spawns left child with `fork()`:
   - Binds `STDOUT_FILENO` (1) to `pipe_fd[1]` via `dup2()`.
   - Closes both pipe ends.
   - Invokes `execvp(left_args[0], left_args)`.
6. Spawns right child with `fork()`:
   - Binds `STDIN_FILENO` (0) to `pipe_fd[0]` via `dup2()`.
   - Closes both pipe ends.
   - Invokes `execvp(right_args[0], right_args)`.
7. Parent closes both `pipe_fd[0]` and `pipe_fd[1]` immediately.
8. If foreground, parent blocks on `waitpid(pid1, ...)` and `waitpid(pid2, ...)`. If background, parent prints both PIDs and continues.

---

### 2.6 I/O Redirection Architecture (`>`, `<`)
Redirection is encapsulated in the `RedirectionInfo` data structure and helper functions:

```c
typedef struct {
    char *cmd_part;
    char *input_file;
    char *output_file;
    int has_input_redirect;
    int has_output_redirect;
} RedirectionInfo;
```

#### Lexical Parsing & Validation:
- Inspects operator counts (`count_out`, `count_in`) to reject repeated operators (`>>`, `<<`).
- Detects operator ordering (`cmd < in > out` vs `cmd > out < in`).
- Splits strings and trims whitespace around command and target filenames.
- Validates that command part and target filenames are non-empty.

#### Child Execution Workflow:
- If `has_input_redirect`:
  - `open(input_file, O_RDONLY)`
  - `dup2(input_fd, STDIN_FILENO)`
  - `close(input_fd)`
- If `has_output_redirect`:
  - `open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644)`
  - `dup2(output_fd, STDOUT_FILENO)`
  - `close(output_fd)`
- Calls `execvp(args[0], args)`.

---

### 2.7 General External Command Execution
For standard commands without redirection or pipes:
1. `parse_command()` tokenizes arguments into `args[]`.
2. `fork()` creates a single child process.
3. Child runs `execvp(args[0], args)`.
4. Parent blocks on `waitpid(pid, NULL, 0)` (or reports PID if background `&`).

---

### 2.8 Process Synchronization & Zombie Prevention
- When a child process terminates, it enters a zombie state (`<defunct>`) until the parent collects its exit status.
- Standard shells can leak memory if background processes are never reaped.
- NexShell executes a non-blocking reaping loop at the top of every REPL iteration:
  ```c
  while (waitpid(-1, NULL, WNOHANG) > 0) {
      // Reap terminated child processes
  }
  ```
- `WNOHANG` ensures the parent never blocks if no background processes have exited.

---

## 3. Data Limits & Buffer Safety
- `MAX_INPUT_SIZE` = 1024 bytes (prevents stack overflow on input).
- `MAX_ARGS` = 64 pointers (supports up to 63 command arguments + trailing `NULL`).
- Strings are trimmed in place with boundary checks preventing out-of-bounds pointer writes.
