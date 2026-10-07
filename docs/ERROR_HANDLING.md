# NexShell - Error Handling & Edge Case Architecture

## Overview
NexShell is designed with defensive programming principles to ensure REPL stability and prevent crash/hang conditions when handling malformed input, invalid system paths, or signal interruptions.

---

## 1. Syntax & Pre-Execution Validation

Syntax validation occurs during string parsing **before** any processes are forked or pipes are created.

### 1.1 Redirection Syntax Errors
- **Missing Output Filename (`>` with no token following)**:
  - Detection: Output redirection symbol `>` detected at end of line or followed by pipe symbol.
  - Error Output: `Error: Missing output filename.`
  - Action: Aborts command execution, returns to REPL prompt.
- **Missing Input Filename (`<` with no token following)**:
  - Detection: Input redirection symbol `<` detected at end of line or followed by pipe symbol.
  - Error Output: `Error: Missing input filename.`
  - Action: Aborts command execution, returns to REPL prompt.

### 1.2 Pipeline Syntax Errors
- **Leading Pipe (`| command`)**:
  - Error Output: `Error: Missing command before '|'.`
  - Action: Aborts execution.
- **Trailing Pipe (`command |`)**:
  - Error Output: `Error: Missing command after '|'.`
  - Action: Aborts execution.
- **Consecutive Pipes (`command1 || command2`)**:
  - Error Output: `Error: Missing command after '|'.`
  - Action: Aborts execution.

---

## 2. System Call Error Handling

### 2.1 Invalid Commands & Execution Failures (`execvp`)
- **Condition**: Binary executable not found in system `PATH` or lacks execute permissions.
- **Handling**: Child process invokes `perror("execvp failed")` and immediately calls `exit(EXIT_FAILURE)`.
- **Parent Behavior**: Parent shell receives child exit state via `waitpid()`, ensuring shell stability.

### 2.2 Invalid Directory Paths (`cd`)
- **Condition**: Target path supplied to `cd` does not exist or is not a directory.
- **Handling**: `chdir(path)` returns `-1`. Parent shell invokes `perror("cd failed")` and remains in current directory.

### 2.3 File Open Errors during Redirection (`open`)
- **Condition**: Input file does not exist (`<`) or output path is unwritable (`>`).
- **Handling**: Child process checks `open()` return descriptor `< 0`, prints error via `perror("open failed")`, and exits child process with code `1`.

### 2.4 Pipe & Fork Failures (`pipe` / `fork`)
- **Pipe Failure**: If `pipe(pipefd)` returns `-1`, prints `perror("pipe failed")`, closes previously opened pipes, and aborts.
- **Fork Failure**: If `fork()` returns `-1`, prints `perror("fork failed")`, terminates active stage children, and reclaims state.

---

## 3. Signal Handling (`SIGINT` / `Ctrl+C`)

### 3.1 Parent REPL
- Parent sets `signal(SIGINT, SIG_IGN)` at startup.
- If `fgets()` is interrupted by a signal and returns `NULL`, `feof(stdin)` is checked. If EOF is not set, `clearerr(stdin)` resets stream state, prints `\n`, and re-renders prompt.

### 3.2 Foreground Child Processes
- Immediately after `fork()`, child process calls `signal(SIGINT, SIG_DFL)`.
- Pressing `Ctrl+C` sends `SIGINT` to foreground process group, terminating child tasks cleanly without affecting parent shell.

### 3.3 Background Child Processes
- Background child processes inherit `signal(SIGINT, SIG_IGN)` to prevent terminal `Ctrl+C` signals from terminating background jobs.

---

## 4. Background Process Management & Reaping

- Background tasks executed with `&` do not block the parent shell.
- To prevent zombie processes, `reap_background_processes()` is executed at the top of each REPL iteration using `waitpid(-1, &status, WNOHANG)`.
- All terminated background process IDs are reaped non-blockingly.
