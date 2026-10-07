# NexShell Input & Output Redirection Architecture Guide

This guide provides an exhaustive technical analysis of the Input and Output Redirection subsystem in **NexShell**. It covers the POSIX process model, kernel file descriptor manipulation, data structures, parsing mechanics, child process isolation, stream substitution via `dup2()`, and best engineering practices.

---

## 1. Fundamentals of Unix I/O Redirection

In Unix and POSIX-compliant operating systems, running processes interact with the outside world through stream channels represented as integer handles called **file descriptors** (FDs). 

Every process maintains an isolated **File Descriptor Table** managed by the kernel:
- **`0` (`STDIN_FILENO`)**: Standard Input — by default connected to the terminal keyboard.
- **`1` (`STDOUT_FILENO`)**: Standard Output — by default connected to the terminal display screen.
- **`2` (`STDERR_FILENO`)**: Standard Error — by default connected to the terminal display screen for diagnostics.

### What Redirection Accomplishes
Redirection alters the entries of this per-process table:
- **Output Redirection (`>`)**: Replaces the binding of file descriptor `1` so that any write operation performed by the process (e.g., `write()`, `printf()`, `puts()`) writes into a target filesystem file rather than the terminal screen.
- **Input Redirection (`<`)**: Replaces the binding of file descriptor `0` so that any read operation performed by the process (e.g., `read()`, `scanf()`, `getchar()`) draws bytes from an existing file rather than waiting for user keyboard keystrokes.
- **Dual Redirection (`<` and `>`)**: Replaces both descriptor `0` and descriptor `1` simultaneously, establishing a pure file-to-file processing filter.

---

## 2. NexShell Redirection Architecture

NexShell implements redirection using a clean, decoupled design structured into three layers:
1. **Data Model (`RedirectionInfo`)**: Encapsulates parsed command components and state flags.
2. **Lexical Parsing & Validation (`parse_redirection`)**: Analyzes the input string, enforces grammar constraints, detects syntax errors, and isolates clean sub-strings.
3. **Process Execution & Descriptor Binding (`execute_child_redirection`)**: Sets up kernel file descriptors inside the cloned child context and replaces the process image via `execvp()`.

```
User Input String (e.g., "cat < input.txt > output.txt")
                          │
                          ▼
            parse_redirection()
     ┌────────────────────┴────────────────────┐
     │ 1. Count occurrences of '<' and '>'     │
     │ 2. Check for repeated/multiple operators│
     │ 3. Detect relative ordering             │
     │ 4. Null-terminate sub-strings in place  │
     │ 5. Validate filenames and command name  │
     └────────────────────┬────────────────────┘
                          │ Populates
                          ▼
                   RedirectionInfo
     ┌─────────────────────────────────────────┐
     │ - cmd_part:            "cat"            │
     │ - input_file:          "input.txt"      │
     │ - output_file:         "output.txt"     │
     │ - has_input_redirect:  1                │
     │ - has_output_redirect: 1                │
     └────────────────────┬────────────────────┘
                          │ Passed to
                          ▼
                     fork()
        ┌─────────────────┴─────────────────┐
        │ Parent                            │ Child (pid == 0)
        ▼                                   ▼
   waitpid()                   execute_child_redirection()
                         ┌──────────────────┴──────────────────┐
                         │ 1. open(input_file, O_RDONLY)       │
                         │ 2. dup2(input_fd, STDIN_FILENO)     │
                         │ 3. close(input_fd)                  │
                         │ 4. open(output_file, O_WRONLY|...)  │
                         │ 5. dup2(output_fd, STDOUT_FILENO)   │
                         │ 6. close(output_fd)                 │
                         │ 7. execvp(args[0], args)            │
                         └─────────────────────────────────────┘
```

---

## 3. The `RedirectionInfo` Data Structure

To avoid sprawling parameter lists and global state, NexShell groups redirection metadata into the `RedirectionInfo` struct:

```c
typedef struct {
    char *cmd_part;            // Pointer to clean command string (e.g., "ls -l")
    char *input_file;          // Target filename for stdin (<) or NULL
    char *output_file;         // Target filename for stdout (>) or NULL
    int has_input_redirect;    // Boolean flag: 1 if '<' requested, 0 otherwise
    int has_output_redirect;   // Boolean flag: 1 if '>' requested, 0 otherwise
} RedirectionInfo;
```

### Purpose of Fields
- `cmd_part`: Contains only the command and its arguments. Operators and filenames are completely excised so `parse_command()` can tokenize clean arguments for `execvp()`.
- `input_file` / `output_file`: Pointers directly to trimmed filename strings within the input buffer.
- `has_input_redirect` / `has_output_redirect`: Explicit boolean flags that allow downstream execution logic to branch cleanly without checking for null pointers.

---

## 4. Parsing and Validation: `parse_redirection()`

Parsing user commands safely requires defensive validation against malformed syntax before any process creation or system calls occur.

### 4.1 Operator Counting & Rejection of Repeated Operators
In standard Unix shells, `>>` denotes append redirection and `<<` denotes here-documents. Because NexShell is an educational shell strictly scoped to single overwrite redirection (`>`) and single input redirection (`<`), multi-character or repeated operators are explicit syntax errors.

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

- If `count_out == 0 && count_in == 0`: Returns `0` immediately. The command has no redirection and continues to normal external execution.
- If `count_out > 1`: Triggers `Error: Multiple or repeated '>' redirection operators.` and returns `-1`. This cleanly rejects `ls >> file`, `cmd > a > b`, and `echo test > > file`.
- If `count_in > 1`: Triggers `Error: Multiple or repeated '<' redirection operators.` and returns `-1`. This cleanly rejects `cat << file`, `cmd < a < b`, and `cat < < file`.

### 4.2 Relative Ordering of Dual Operators
When both `<` and `>` are present in a single command, their relative order matters:

```c
char *out_redirect_ptr = strchr(input_str, '>');
char *in_redirect_ptr = strchr(input_str, '<');

if (out_redirect_ptr != NULL && in_redirect_ptr != NULL) {
    if (in_redirect_ptr < out_redirect_ptr) {
        // Format: cmd < in_file > out_file
        *in_redirect_ptr = '\0';
        *out_redirect_ptr = '\0';
        redir->cmd_part = trim_whitespace(input_str);
        redir->input_file = trim_whitespace(in_redirect_ptr + 1);
        redir->output_file = trim_whitespace(out_redirect_ptr + 1);
    } else {
        // Format: cmd > out_file < in_file
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

By placing null terminators (`\0`) at the operator pointers, the original buffer is segmented in place without requiring dynamic heap allocations (`malloc`), eliminating memory leak hazards.

### 4.3 Tight Syntax Support
Because pointers are identified using `strchr()`, whitespace is not required around operators:
- `ls>output.txt` &rarr; `cmd_part` becomes `"ls"`, `output_file` becomes `"output.txt"`.
- `cat<input.txt` &rarr; `cmd_part` becomes `"cat"`, `input_file` becomes `"input.txt"`.
- `cat<in.txt>out.txt` &rarr; `cmd_part` becomes `"cat"`, `input_file` becomes `"in.txt"`, `output_file` becomes `"out.txt"`.

### 4.4 Filename & Command Validation
Once tokens are isolated, their string lengths are evaluated:
1. **Missing Output Filename**: If `has_output_redirect` is active but `strlen(output_file) == 0`, prints `Error: Missing output filename.` and returns `-1`.
2. **Missing Input Filename**: If `has_input_redirect` is active but `strlen(input_file) == 0`, prints `Error: Missing input filename.` and returns `-1`.
3. **Missing Command**: If `strlen(cmd_part) == 0`, identifies whether `<` or `>` was missing a preceding command and prints `Error: Missing command before '>'.` or `Error: Missing command before '<'.`.

---

## 5. Child Execution & Kernel Stream Manipulation

Once parsing succeeds, the parent shell calls `fork()`. The child process executes `execute_child_redirection()` to perform stream redirection before `execvp()`.

```c
static void execute_child_redirection(const RedirectionInfo *redir, char **args)
```

### 5.1 Input Redirection Step-by-Step
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

1. **`open(redir->input_file, O_RDONLY)`**:
   - Opens the source file in read-only mode.
   - The kernel allocates the lowest available integer in the process FD table (typically `3`).
   - If the file does not exist, `open()` returns `-1`. The child calls `perror("open failed")` and exits with code `1`.
2. **`dup2(input_fd, STDIN_FILENO)`**:
   - Atomically closes descriptor `0` (the keyboard input) and duplicates `input_fd` into descriptor `0`.
   - Descriptor `0` now points directly to the open file description in the kernel.
3. **`close(input_fd)`**:
   - Closes descriptor `3`.
   - Descriptor `0` remains active and pointing to the file. Closing `3` is essential to prevent descriptor leaks.

### 5.2 Output Redirection Step-by-Step
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

1. **`open(redir->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644)`**:
   - `O_WRONLY`: Ensures the file descriptor can only write data.
   - `O_CREAT`: If the destination file does not exist on disk, the kernel creates it.
   - `O_TRUNC`: If the file already exists, its length is truncated to 0 bytes before writing. This guarantees overwrite behavior rather than append behavior.
   - `0644` Mode: Sets standard POSIX read/write permissions for the user and read-only permissions for group and others (`-rw-r--r--`).
2. **`dup2(output_fd, STDOUT_FILENO)`**:
   - Atomically replaces descriptor `1` (terminal screen) with `output_fd`.
   - Any future calls to `write(1, ...)` or standard C library output functions will write directly into the file.
3. **`close(output_fd)`**:
   - Closes the redundant descriptor (e.g., `3` or `4`), leaving descriptor `1` as the sole handle.

### 5.3 Program Execution via `execvp()`
```c
execvp(args[0], args);
perror("execvp failed");
exit(1);
```

When `execvp()` is invoked:
- The Linux kernel replaces the child process's text, data, heap, and stack segments with the new binary image.
- **Crucially, file descriptors preserved across `execvp()` remain intact** (unless `FD_CLOEXEC` was set).
- Therefore, the newly executed binary inherits descriptor `0` pointing to `input_file` and descriptor `1` pointing to `output_file`. The program runs unaware of the redirection, reading standard input and writing standard output naturally.
- If `execvp()` fails (e.g., command binary not found), it returns `-1`. The child reports `execvp failed: No such file or directory` and terminates via `exit(1)`.

---

## 6. Process Isolation: Why Redirection Belongs in the Child

A fundamental rule of shell design is that **stream redirection must occur inside the child process, never in the parent shell**.

| Action | Executed in Child Process | If Executed in Parent Process |
| :--- | :--- | :--- |
| **`open()` failure** | Child prints error and terminates (`exit(1)`). Parent reaps child and prompt continues normally. | Shell cannot execute command; if parent aborts, entire shell session terminates. |
| **`dup2()` to STDOUT** | Only the child's descriptor `1` is replaced. Child outputs to file. | Parent's descriptor `1` is replaced. `printf("NexShell> ")` writes to file, disappearing from terminal! |
| **`dup2()` to STDIN** | Only the child's descriptor `0` is replaced. Child reads from file. | Parent's descriptor `0` is replaced. Shell stops accepting user keyboard input! |

By confining descriptor manipulation to the child process between `fork()` and `execvp()`, the parent NexShell environment remains completely stable and uncorrupted.

---

## 7. Error Handling Matrix

| Failure Point | System Call / Function | Action Taken | Shell Impact |
| :--- | :--- | :--- | :--- |
| Repeated operator (`>>` or `<<`) | `parse_redirection()` | Prints `Error: Multiple or repeated ... operators.` | `fork()` is skipped; prompt returns immediately. |
| Missing output filename (`ls >`) | `parse_redirection()` | Prints `Error: Missing output filename.` | `fork()` is skipped; prompt returns immediately. |
| Missing input filename (`cat <`) | `parse_redirection()` | Prints `Error: Missing input filename.` | `fork()` is skipped; prompt returns immediately. |
| Missing command (`> out.txt`) | `parse_redirection()` | Prints `Error: Missing command before '>'.` | `fork()` is skipped; prompt returns immediately. |
| Input file does not exist | `open(..., O_RDONLY)` | Child prints `open failed: No such file or directory` via `perror()`, calls `exit(1)` | Parent reaps child with `waitpid()`; prompt redisplays safely. |
| Insufficient read permissions | `open(..., O_RDONLY)` | Child prints `open failed: Permission denied`, calls `exit(1)` | Parent reaps child with `waitpid()`; prompt redisplays safely. |
| Read-only filesystem / Invalid path | `open(..., O_WRONLY\|...)` | Child prints `open failed: No such file or directory` or `Permission denied` | Parent reaps child with `waitpid()`; prompt redisplays safely. |
| Command binary not found | `execvp()` | Child prints `execvp failed: No such file or directory`, calls `exit(1)` | Output file remains created/truncated; shell returns to prompt. |

---

## 8. Summary of Engineering Best Practices in NexShell

1. **Zero Dynamic Allocation Overhead**: Redirection parsing mutates the in-memory command buffer using pointers and null bytes, avoiding heap allocation (`malloc`/`free`) bugs.
2. **Defensive Pre-Fork Checks**: Syntax errors are caught and reported in user space before expending kernel resources to clone a process via `fork()`.
3. **Immediate Descriptor Cleanup**: File descriptors are closed immediately following duplication, leaving no dormant descriptors open across `execvp()`.
4. **Complete Fault Isolation**: Any subsystem failure in child setup terminates solely that child, guaranteeing 100% parent shell uptime.
