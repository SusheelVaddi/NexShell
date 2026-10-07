# NexShell Limitations & Future Technical Scope

This document details the intentional design limitations of the current **NexShell** implementation, explains why these constraints exist from an educational perspective, and outlines detailed architectural roadmaps for future enhancements.

---

## 1. Summary of Current Limitations

| Feature / Capability | Current NexShell Support | Full POSIX Shell Standard | Impact / Limitation |
| :--- | :--- | :--- | :--- |
| **Pipeline Length** | Single-stage pipe only (`cmd1 \| cmd2`) | Arbitrary N-stage chains (`a \| b \| c \| ...`) | Cannot chain 3 or more commands |
| **Operator Combinations** | Separate redirection OR pipe | Combined (`cat in \| grep x > out`) | Cannot redirect pipe output on single line |
| **Argument Quoting** | Basic space/tab delimiter | Escaped quotes (`"..."`, `'...'`, `\ `) | Whitespace inside strings split into args |
| **Globbing / Wildcards** | Exact binary filenames | Wildcards (`*`, `?`, `[a-z]`) | Cannot run `ls *.c` directly |
| **Job Control** | Background execution (`&`) | `jobs`, `fg`, `bg`, process groups | Cannot bring background jobs to foreground |
| **Signal Handling** | Default OS signal actions | Custom `SIGINT`, `SIGTSTP` trapping | `Ctrl+C` terminates interactive shell |
| **Command History** | Single-line `fgets()` | Arrow-key navigation, reverse search | Cannot recall past commands with Up arrow |

---

## 2. In-Depth Analysis of Limitations & Future Implementation

### 2.1 Multi-Stage Pipeline Chaining (`cmd1 | cmd2 | ... | cmdN`)
- **Current Limitation**: NexShell uses a single `pipe_fd[2]` array and forks exactly two child processes.
- **Why It Matters**: Complex command pipelines in real-world Unix scripting frequently require three or more stages (e.g., `cat access.log | grep 404 | awk '{print $7}' | sort | uniq -c`).
- **Future Implementation**:
  - Allocate an array of `N - 1` pipes: `int pipes[N - 1][2]`.
  - Loop through each stage `i` from `0` to `N - 1`.
  - For stage `0`: redirect `stdout` to `pipes[0][1]`.
  - For intermediate stage `i`: redirect `stdin` to `pipes[i - 1][0]` and `stdout` to `pipes[i][1]`.
  - For stage `N - 1`: redirect `stdin` to `pipes[N - 2][0]`.
  - Close all unused pipe file descriptors in all child processes and parent.

---

### 2.2 Operator Combination (Piping + Redirection)
- **Current Limitation**: NexShell dispatches pipelines before checking for redirection operators. If both `|` and `>` appear on the same command line, only the pipe is executed.
- **Why It Matters**: Directing filtered pipeline streams into log files on a single line is a standard shell pattern.
- **Future Implementation**:
  - Implement a recursive-descent or staged token parser that parses redirection operators on the individual left and right subcommands of a pipeline.

---

### 2.3 Shell Quoting & Escape Sequences (`"..."` / `'...'`)
- **Current Limitation**: `parse_command()` uses `strtok(cmd, " \t")`, which unconditionally splits tokens at any space or tab.
- **Why It Matters**: Commands with spaced arguments (e.g., `echo "Hello World"` or `mkdir "My Folder"`) treat `World` and `Folder` as separate arguments.
- **Future Implementation**:
  - Replace `strtok()` with a custom state machine parser tracking quote states: `STATE_NORMAL`, `STATE_IN_SINGLE_QUOTE`, `STATE_IN_DOUBLE_QUOTE`, and `STATE_ESCAPED`.

---

### 2.4 Filesystem Wildcard Expansion (Globbing)
- **Current Limitation**: `execvp()` receives the literal pattern string `*.c` without expansion because globbing is a shell feature, not a kernel feature.
- **Why It Matters**: Running `ls *.c` passes `*.c` directly to `ls`, which fails if no literal file named `*.c` exists.
- **Future Implementation**:
  - Integrate the POSIX `glob()` library function (`<glob.h>`) to expand wildcard patterns into matching file arrays before populating `args[]`.

---

### 2.5 Interactive Signal Trapping (`SIGINT`, `SIGTSTP`)
- **Current Limitation**: Pressing `Ctrl+C` sends `SIGINT` to the shell process group, terminating the interactive NexShell session.
- **Why It Matters**: In standard shells, `Ctrl+C` cancels only the currently running foreground child process, returning a fresh prompt to the user.
- **Future Implementation**:
  - Set custom signal actions in the parent process using `sigaction()`:
    ```c
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);
    ```

---

### 2.6 Command History & Auto-Completion (GNU Readline)
- **Current Limitation**: `fgets()` reads raw characters without support for arrow keys (which emit ANSI escape sequences like `^[[A`).
- **Why It Matters**: Enhances usability during interactive terminal sessions.
- **Future Implementation**:
  - Link against GNU Readline (`-lreadline`) and replace `fgets()` with `readline("NexShell> ")` and `add_history(line)`.
