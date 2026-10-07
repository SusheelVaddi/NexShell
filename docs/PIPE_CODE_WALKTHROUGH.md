# NexShell Pipe Code Walkthrough

This document presents a line-by-line technical walkthrough of the pipeline execution logic implemented in `main.c` (lines 116–224).

---

## 1. Code Location and Functionality

In `main.c`, pipeline detection and execution occurs after stripping trailing background symbols (`&`) and checking for parent built-in commands (`exit` and `cd`).

```c
// main.c: Lines 116–123
char *pipe_ptr = strchr(trimmed_input, '|');
if (pipe_ptr != NULL) {
    // Split input into left command and right command
    *pipe_ptr = '\0';
    char *left_cmd_part = trim_whitespace(trimmed_input);
    char *right_cmd_part = trim_whitespace(pipe_ptr + 1);
```

---

## 2. Detailed Execution Trace

### Phase 1: String Interception & Parsing

1. **Operator Location**:
   - `strchr(trimmed_input, '|')` searches for the first pipe symbol `|`.
   - If `pipe_ptr != NULL`, the shell enters the pipeline execution branch.

2. **Null Termination & Component Splitting**:
   - `*pipe_ptr = '\0'` replaces the `|` character with a null byte in place.
   - This cleanly divides the single input buffer into two separate null-terminated strings:
     - `trimmed_input`: Starts at the beginning of the command string up to `|`.
     - `pipe_ptr + 1`: Starts immediately after `|`.

3. **Whitespace Trimming**:
   - `trim_whitespace(trimmed_input)` returns `left_cmd_part`.
   - `trim_whitespace(pipe_ptr + 1)` returns `right_cmd_part`.

```c
// main.c: Lines 124–134
if (strlen(right_cmd_part) == 0) {
    printf("Error: Missing command after '|'.\n");
    continue;
}

if (strlen(left_cmd_part) == 0) {
    printf("Error: Missing command before '|'.\n");
    continue;
}
```

4. **Syntax Validation**:
   - If either command string is empty (e.g. `| sort` or `ls |`), NexShell outputs a syntax error message and aborts execution, restarting the REPL loop via `continue`.

---

### Phase 2: Command Argument Tokenization

```c
// main.c: Lines 136–149
char *left_args[MAX_ARGS];
int left_count = parse_command(left_cmd_part, left_args, MAX_ARGS);
if (left_count == 0) {
    printf("Error: Invalid command before '|'.\n");
    continue;
}

char *right_args[MAX_ARGS];
int right_count = parse_command(right_cmd_part, right_args, MAX_ARGS);
if (right_count == 0) {
    printf("Error: Invalid command after '|'.\n");
    continue;
}
```

- `parse_command()` tokenizes `left_cmd_part` into null-terminated string array `left_args[]`.
- `parse_command()` tokenizes `right_cmd_part` into `right_args[]`.
- Maximum arguments allowed per subcommand: `MAX_ARGS` (64).
- Returns the token count, ensuring `args[count] == NULL` as required by `execvp()`.

---

### Phase 3: Kernel Pipe Allocation

```c
// main.c: Lines 151–157
int pipe_fd[2];
if (pipe(pipe_fd) < 0) {
    perror("pipe failed");
    continue;
}
```

- Declares integer array `pipe_fd[2]`.
- Calls POSIX system call `pipe(pipe_fd)`.
- On success:
  - `pipe_fd[0]` receives read file descriptor (e.g., FD 3).
  - `pipe_fd[1]` receives write file descriptor (e.g., FD 4).
- On failure: prints error via `perror` and resumes REPL.

---

### Phase 4: Left Child Process Spawning (`pid1`)

```c
// main.c: Lines 159–183
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
```

#### Step-by-Step in Left Child (`pid1 == 0`):
1. `dup2(pipe_fd[1], STDOUT_FILENO)` overwrites standard output (FD 1) with `pipe_fd[1]`.
2. Both `pipe_fd[0]` and `pipe_fd[1]` are closed (`close(pipe_fd[0])`, `close(pipe_fd[1])`). FD 1 remains open pointing to the write end of the kernel pipe.
3. `execvp(left_args[0], left_args)` replaces the process image with the target binary.
4. If `execvp()` succeeds, code execution continues inside the new executable binary image.
5. If `execvp()` fails, `perror()` prints the reason to stderr and `exit(1)` terminates the child process immediately.

---

### Phase 5: Right Child Process Spawning (`pid2`)

```c
// main.c: Lines 185–210
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
```

#### Step-by-Step in Right Child (`pid2 == 0`):
1. `dup2(pipe_fd[0], STDIN_FILENO)` overwrites standard input (FD 0) with `pipe_fd[0]`.
2. `close(pipe_fd[0])` and `close(pipe_fd[1])` release unused descriptors. FD 0 remains open pointing to the read end of the pipe.
3. `execvp(right_args[0], right_args)` replaces process image with the second target binary.
4. If `execvp()` fails, `perror()` prints error and `exit(1)` terminates the child.

---

### Phase 6: Parent Cleanup & Process Synchronization

```c
// main.c: Lines 212–223
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
```

#### Parent Actions:
1. **Closing Pipe Ends**: The parent closes both `pipe_fd[0]` and `pipe_fd[1]` descriptors. This ensures that the only remaining open write descriptor is held by Left Child. When Left Child exits, all write ends are closed, sending EOF to Right Child.
2. **Foreground Synchronization**: If `is_background == 0`, parent calls `waitpid(pid1, NULL, 0)` followed by `waitpid(pid2, NULL, 0)`, blocking until both children finish execution.
3. **Background Execution**: If `is_background == 1`, parent prints both PIDs (`[Background process started: PIDs 1234, 1235]`) and skips `waitpid()`, returning immediately to the prompt loop.

---

## 3. Descriptor Ownership Matrix

| Process Context | `pipe_fd[0]` (Read End) | `pipe_fd[1]` (Write End) | `STDIN_FILENO` (0) | `STDOUT_FILENO` (1) |
| :--- | :--- | :--- | :--- | :--- |
| **Parent (Before Fork)** | Open | Open | Terminal (`/dev/pts/X`) | Terminal (`/dev/pts/X`) |
| **Left Child (`pid1`)** | Closed via `close()` | Closed after `dup2` | Terminal (`/dev/pts/X`) | **Kernel Pipe Write End** |
| **Right Child (`pid2`)**| Closed after `dup2` | Closed via `close()` | **Kernel Pipe Read End** | Terminal (`/dev/pts/X`) |
| **Parent (After Fork)** | Closed via `close()` | Closed via `close()` | Terminal (`/dev/pts/X`) | Terminal (`/dev/pts/X`) |
