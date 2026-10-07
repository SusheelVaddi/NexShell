# NexShell Background Execution Guide

This document presents a comprehensive technical guide to background process execution (`command &`) in **NexShell**. It details asynchronous process management, zombie process lifecycle, non-blocking reaping mechanics (`WNOHANG`), and architectural comparisons with full job-control shells.

---

## 1. Foreground vs. Background Execution

In Unix operating systems, commands execute either in the **foreground** or in the **background**:

- **Foreground Execution**: The shell parent process spawns a child process and immediately blocks (`waitpid(pid, NULL, 0)`), yielding terminal control to the child until it terminates. The user cannot enter new shell commands until the child finishes.
- **Background Execution (`&`)**: The shell parent process spawns a child process but **bypasses blocking `waitpid()`**. The parent prints the child's Process ID (PID) and immediately returns control to the interactive REPL prompt, allowing the user to issue subsequent commands while the background child runs concurrently.

---

## 2. Background Execution Logic in NexShell

Background execution handling in `main.c` involves four distinct phases:

### Phase 1: Symbol Detection & Sanitization
```c
// main.c: Lines 66–78
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
```
1. `strrchr(input, '&')` locates the last occurrence of `&`.
2. Validates that only whitespace follows `&` up to the string null terminator (`\0`).
3. Sets `is_background = 1` and overwrites `&` with `\0`, sanitizing the command string so `execvp()` receives clean arguments without `&`.

---

### Phase 2: Process Creation & Non-Blocking Parent Return
```c
// main.c: Lines 373–378
if (is_background) {
    printf("[Background process started: PID %d]\n", pid);
} else {
    // Foreground process: wait for child process to complete
    waitpid(pid, NULL, 0);
}
```
- If `is_background == 1`, the parent shell prints `[Background process started: PID <pid>]`.
- The parent skips `waitpid(pid, NULL, 0)` and returns to the top of the `while (1)` loop, displaying `NexShell> `.

---

### Phase 3: Zombie Processes & Kernel Lifecycle

When a child process finishes execution, the kernel does not immediately delete its `task_struct` entry from the process table. Instead, the process enters the **Zombie** state (`Z` state in `ps` output).

```
+------------------+     fork()     +-------------------+     execvp()     +-------------------+
|  Parent Shell    | -------------> | Child Created     | ---------------> | Child Running     |
+------------------+                +-------------------+                  +-------------------+
         |                                                                           |
         |                                                                           | exit() / Terminate
         |                                                                           v
         |                          +-------------------+                  +-------------------+
         |  Reaped via waitpid()    | Kernel Cleaned    | <--------------- | Zombie Process    |
         +------------------------- | (Process Removed) |                  | (State: Z)        |
                                    +-------------------+                  +-------------------+
```

#### Why Zombie Processes Occur:
The kernel keeps the terminated process PID, exit status, and resource usage stats in memory until the parent process reads them via `waitpid()`. If the parent never calls `waitpid()`, zombie process entries accumulate, eventually exhausting available process IDs (`PID_MAX`).

---

### Phase 4: Asynchronous Reaping with `WNOHANG`

To prevent zombie accumulation without blocking the shell prompt, NexShell executes a non-blocking reaping loop at the start of every REPL iteration:

```c
// main.c: Lines 47–49
while (waitpid(-1, NULL, WNOHANG) > 0) {
    // Reap zombie background processes
}
```

#### Breakdown of `waitpid(-1, NULL, WNOHANG)`:
- `-1`: Instructs `waitpid` to check state changes for **any** child process belonging to the parent shell.
- `NULL`: Ignores the exit status code (status storage pointer set to null).
- `WNOHANG`: **Non-blocking flag**. Instructs `waitpid` to return immediately:
  - Returns `PID > 0` if a terminated child process was found and reaped.
  - Returns `0` if child processes exist but none have changed state (none terminated).
  - Returns `-1` if no child processes exist.
- The `while` loop continues reaping until all terminated background children are cleaned up.

---

## 3. Parent / Child Execution Timeline

```
Time  Parent Shell (NexShell)                     Child Process (sleep 5)
  |   -----------------------                     -----------------------
  |   1. Read "sleep 5 &"
  |   2. Parse '&' -> is_background = 1
  |   3. Call fork() ---------------------------> 1. Child process created (PID 4520)
  |   4. Print "[Background process..."           2. Call execvp("sleep", ["sleep", "5"])
  |   5. Re-display "NexShell> "                  3. Process enters sleep state
  |   6. User enters "pwd"                        |
  |   7. Execute "pwd" (foreground)               |  (Sleeping...)
  |   8. Print "/home/user/NexShell"              |
  |   9. Display "NexShell> "                     |
  |   |                                           4. sleep 5 completes & calls exit()
  |   |                                           5. Process enters ZOMBIE state
  |   10. User presses ENTER                      |
  |   11. Loop reaches waitpid(-1,NULL,WNOHANG) -> 6. Reaped! PID 4520 removed from kernel.
  v
```

---

## 4. Race Conditions & Terminal I/O Considerations

### 1. Terminal Output Collision
Because NexShell background processes share standard output (`STDOUT_FILENO`) with the parent terminal, a background process that prints text (e.g. `echo hello &` or `ls &`) will write directly to the screen while the user is typing at the `NexShell> ` prompt.

### 2. Immediate Termination Race
If a background command executes extremely fast (e.g., `echo quick &`), the child may terminate before the parent shell even prints `[Background process started: PID <pid>]`. The child becomes a zombie for a fraction of a millisecond until the parent's next loop iteration calls `waitpid(..., WNOHANG)`.

---

## 5. Architectural Limitations vs. Full Job-Control Shells

NexShell provides lightweight background execution, but does not implement full POSIX job control:

| Feature | NexShell | Full Shell (Bash / Zsh) |
| :--- | :--- | :--- |
| **Background Spawning (`&`)** | Supported | Supported |
| **Non-blocking Zombie Reaping** | Supported via `WNOHANG` | Supported via `SIGCHLD` signal handler |
| **Job Control Built-ins (`jobs`, `fg`, `bg`)** | Not Implemented | Supported |
| **Process Group Isolation (`setpgid`)** | Background jobs remain in shell process group | Background jobs assigned dedicated Process Group ID (PGID) |
| **Terminal Control Assignment (`tcsetpgrp`)**| Not Implemented | Shell yields terminal control to foreground PGID |
| **Signal Suspend/Resume (`Ctrl+Z`, `SIGTSTP`)**| Terminal signal suspends entire shell process | Shell intercepts `SIGTSTP` and moves foreground job to background |
