# NexShell Redirection Code Walkthrough & Execution Lifecycle

This document provides a line-by-line, function-by-function technical walkthrough of the Input and Output Redirection subsystem in **NexShell** ([`main.c`](file:///Users/jaswant/Downloads/NexShell/NexShell/main.c)). It tracks the complete execution lifecycle of a command from the initial user input to process termination.

---

## 1. High-Level Execution Sequence

When a command containing redirection operators is entered (e.g., `cat < input.txt > output.txt`), the complete lifecycle unfolds as follows:

```
                  ┌──────────────────────────────┐
                  │ 1. User enters command line  │
                  └──────────────┬───────────────┘
                                 │
                                 ▼
                  ┌──────────────────────────────┐
                  │ 2. fgets() reads into input[]│
                  │    Strip newline (\n)        │
                  │    Check for background '&'  │
                  │    trim_whitespace()         │
                  └──────────────┬───────────────┘
                                 │
                                 ▼
                  ┌──────────────────────────────┐
                  │ 3. Check Built-ins & Pipes   │
                  │    - exit, cd                │
                  │    - pipe operator '|'       │
                  └──────────────┬───────────────┘
                                 │
                                 ▼
                  ┌──────────────────────────────┐
                  │ 4. parse_redirection()       │
                  │    - Count '<' and '>'       │
                  │    - Reject repeated (>>, <<)│
                  │    - Split pointers with \0  │
                  │    - Validate strings        │
                  └──────────────┬───────────────┘
                                 │
            ┌────────────────────┴────────────────────┐
            │ redir_status < 0                        │ redir_status > 0
            ▼                                         ▼
┌──────────────────────────────┐          ┌──────────────────────────────┐
│ Syntax Error Detected        │          │ Valid Redirection Detected   │
│ - Skip execution             │          │ - parse_command() on cmd_part│
│ - Display prompt immediately │          │ - Call fork()                │
└──────────────────────────────┘          └──────────────┬───────────────┘
                                                         │
                        ┌────────────────────────────────┴────────────────────────────────┐
                        │                                                                 │
                        ▼ Parent Process (pid > 0)                                        ▼ Child Process (pid == 0)
         ┌──────────────────────────────┐                                  ┌──────────────────────────────┐
         │ If is_background:            │                                  │ execute_child_redirection()  │
         │   Print "[Background PID]"   │                                  │ 1. Setup Input:              │
         │ Else:                        │                                  │    open(in, O_RDONLY)        │
         │   waitpid(pid, NULL, 0)      │                                  │    dup2(in_fd, STDIN_FILENO) │
         │   Wait for child termination │                                  │    close(in_fd)              │
         └──────────────┬───────────────┘                                  │ 2. Setup Output:             │
                        │                                                  │    open(out, O_WRONLY|...)   │
                        ▼                                                  │    dup2(out_fd, STDOUT_FILENO│
         ┌──────────────────────────────┐                                  │    close(out_fd)             │
         │ Prompt redisplayed:          │                                  │ 3. execvp(args[0], args)     │
         │ NexShell>                    │                                  └──────────────┬───────────────┘
         └──────────────────────────────┘                                                 │
                                                                                          ▼ Process Replaced
                                                                           ┌──────────────────────────────┐
                                                                           │ Binary executes with bound   │
                                                                           │ FDs. On termination, kernel  │
                                                                           │ closes all open streams.     │
                                                                           └──────────────────────────────┘
```

---

## 2. Data Structure: `RedirectionInfo`

Defined at lines 42–48 of [`main.c`](file:///Users/jaswant/Downloads/NexShell/NexShell/main.c):

```c
typedef struct {
    char *cmd_part;
    char *input_file;
    char *output_file;
    int has_input_redirect;
    int has_output_redirect;
} RedirectionInfo;
```

### Technical Rationale
- Decouples parsing from process spawning and descriptor manipulation.
- Encapsulates state flags (`has_input_redirect`, `has_output_redirect`) so consumers don't rely on fragile string pointer checks.
- All pointer members point directly to sliced segments of the pre-allocated `input[]` buffer, eliminating memory allocation overhead.

---

## 3. Function Walkthrough: `parse_redirection()`

Defined at lines 51–141 of [`main.c`](file:///Users/jaswant/Downloads/NexShell/NexShell/main.c):

```c
static int parse_redirection(char *input_str, RedirectionInfo *redir)
```

### 3.1 Operator Counting Loop (Lines 53–62)
```c
int count_out = 0;
int count_in = 0;
for (int i = 0; input_str[i] != '\0'; i++) {
    if (input_str[i] == '>') {
        count_out++;
    } else if (input_str[i] == '<') {
        count_in++;
    }
}
```
- **Line 53–54**: Initializes occurrence counters for `>` and `<`.
- **Line 55–61**: Scans every character in the string buffer.
- **Line 64–66**: If both counts are `0`, returns `0` immediately. The command contains no redirection; the shell falls through to normal external command execution.

### 3.2 Repeated Operator Validation (Lines 69–76)
```c
if (count_out > 1) {
    printf("Error: Multiple or repeated '>' redirection operators.\n");
    return -1;
}
if (count_in > 1) {
    printf("Error: Multiple or repeated '<' redirection operators.\n");
    return -1;
}
```
- Traps and rejects unsupported append (`>>`), here-document (`<<`), or multi-file constructs (`echo a > f1 > f2`).
- Returns `-1` so `main()` can skip execution without spawning processes.

### 3.3 Structure Initialization (Lines 79–83)
```c
redir->cmd_part = NULL;
redir->input_file = NULL;
redir->output_file = NULL;
redir->has_input_redirect = 0;
redir->has_output_redirect = 0;
```
- Zeroes out structure state before pointer assignment.

### 3.4 Slicing Operators and Ordering (Lines 85–119)
```c
char *out_redirect_ptr = strchr(input_str, '>');
char *in_redirect_ptr = strchr(input_str, '<');
```
Uses `strchr()` to locate exact pointer addresses of the operator characters.

#### Case 1: Both Operators Present (Dual Redirection)
```c
if (out_redirect_ptr != NULL && in_redirect_ptr != NULL) {
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
}
```
- Null bytes (`\0`) replace `<` and `>` directly in the string.
- `trim_whitespace()` strips spaces from the isolated tokens.
- Activates both redirection boolean flags.

#### Case 2 & 3: Single Operator Present
```c
} else if (out_redirect_ptr != NULL) {
    *out_redirect_ptr = '\0';
    redir->cmd_part = trim_whitespace(input_str);
    redir->output_file = trim_whitespace(out_redirect_ptr + 1);
    redir->has_output_redirect = 1;
} else {
    *in_redirect_ptr = '\0';
    redir->cmd_part = trim_whitespace(input_str);
    redir->input_file = trim_whitespace(in_redirect_ptr + 1);
    redir->has_input_redirect = 1;
}
```
- Slices the string cleanly into `cmd_part` and the single target file.

### 3.5 Filename & Command Validation (Lines 122–139)
```c
if (redir->has_output_redirect && strlen(redir->output_file) == 0) {
    printf("Error: Missing output filename.\n");
    return -1;
}
if (redir->has_input_redirect && strlen(redir->input_file) == 0) {
    printf("Error: Missing input filename.\n");
    return -1;
}
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
```
- Guarantees that neither `output_file`, `input_file`, nor `cmd_part` is empty.
- Returns `1` upon complete parsing and validation success.

---

## 4. Function Walkthrough: `execute_child_redirection()`

Defined at lines 143–194 of [`main.c`](file:///Users/jaswant/Downloads/NexShell/NexShell/main.c):

```c
static void execute_child_redirection(const RedirectionInfo *redir, char **args)
```
Executed exclusively inside the child process context (`pid == 0`).

### 4.1 Input Redirection Handling (Lines 145–163)
```c
if (redir->has_input_redirect) {
    int input_fd = open(redir->input_file, O_RDONLY);
    if (input_fd < 0) {
        perror("open failed");
        exit(1);
    }
    if (dup2(input_fd, STDIN_FILENO) < 0) {
        perror("dup2 failed");
        close(input_fd);
        exit(1);
    }
    close(input_fd);
}
```
1. `open(..., O_RDONLY)`: Obtains a read-only file descriptor pointing to the input file.
2. Error Check: If `input_fd < 0`, `perror("open failed")` outputs the kernel error (e.g. `No such file or directory`) to stderr, and `exit(1)` terminates the child.
3. `dup2(input_fd, STDIN_FILENO)`: Atomically duplicates `input_fd` into descriptor `0`.
4. `close(input_fd)`: Closes the original descriptor to avoid descriptor leaks.

### 4.2 Output Redirection Handling (Lines 166–186)
```c
if (redir->has_output_redirect) {
    int output_fd = open(redir->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output_fd < 0) {
        perror("open failed");
        exit(1);
    }
    if (dup2(output_fd, STDOUT_FILENO) < 0) {
        perror("dup2 failed");
        close(output_fd);
        exit(1);
    }
    close(output_fd);
}
```
1. `open(..., O_WRONLY | O_CREAT | O_TRUNC, 0644)`: Opens the file for writing, creates it if absent with permissions `0644`, and truncates it to 0 bytes if present.
2. Error Check: If `output_fd < 0`, outputs error via `perror()` and terminates via `exit(1)`.
3. `dup2(output_fd, STDOUT_FILENO)`: Duplicates `output_fd` into descriptor `1`.
4. `close(output_fd)`: Closes the original descriptor.

### 4.3 Program Image Execution (Lines 188–193)
```c
execvp(args[0], args);
perror("execvp failed");
exit(1);
```
- `execvp()` loads the executable binary and passes the argument array `args`.
- If `execvp()` returns, execution failed (e.g., binary not found). `perror("execvp failed")` outputs the error to `stderr` (which remains connected to the terminal), and `exit(1)` exits.

---

## 5. Main Execution Loop Integration

In `main()` at lines 380–416:

```c
RedirectionInfo redir;
int redir_status = parse_redirection(trimmed_input, &redir);

if (redir_status < 0) {
    continue;
} else if (redir_status > 0) {
    char *args[MAX_ARGS];
    int arg_count = parse_command(redir.cmd_part, args, MAX_ARGS);

    if (arg_count == 0) {
        printf("Error: Invalid command before redirection.\n");
        continue;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        continue;
    } else if (pid == 0) {
        execute_child_redirection(&redir, args);
    } else {
        if (is_background) {
            printf("[Background process started: PID %d]\n", pid);
        } else {
            waitpid(pid, NULL, 0);
        }
    }
    continue;
}
```

### Parent vs. Child Coordination
- **Parent Process**:
  - `fork()` returns the child PID (`> 0`).
  - If `is_background` is active (`&`), parent prints PID and immediately loops to prompt.
  - If foreground, parent blocks via `waitpid(pid, NULL, 0)` until the child terminates.
  - The parent's stdin/stdout descriptors were never altered, preserving normal shell interaction.
- **Child Process**:
  - `fork()` returns `0`.
  - Configures FDs and calls `execvp()`.
  - Child memory and file descriptor state are isolated.

This clean separation guarantees that NexShell remains robust, memory-safe, and crash-resilient across all command executions.
