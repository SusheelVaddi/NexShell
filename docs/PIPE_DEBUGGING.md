# NexShell Pipe Debugging & Troubleshooting Guide

This guide provides system-level debugging methodologies, diagnostic tools, and failure resolutions for command pipeline (`|`) issues in **NexShell**.

---

## 1. Failure Modes & Root Cause Analysis

### Problem 1: Second Command Hangs Indefinitely (EOF Never Arrives)
- **Symptom**: Executing `ls | sort` causes the shell to freeze after outputting nothing. The cursor remains blocked.
- **Root Cause**: The write end of the pipe (`pipe_fd[1]`) remains open in at least one process (either the parent shell or the right child process). Because the reference count for `pipe_fd[1]` is greater than zero, the operating system kernel never sends an End-of-File (EOF / 0 bytes) to `sort`.
- **Code Verification**:
  Ensure the parent process calls `close(pipe_fd[1])` immediately after forking both children:
  ```c
  // Correct implementation in main.c
  close(pipe_fd[0]);
  close(pipe_fd[1]);
  ```
  And verify that both children close `pipe_fd[1]` after `dup2()`:
  ```c
  close(pipe_fd[0]);
  close(pipe_fd[1]);
  ```

---

### Problem 2: Pipeline Produces No Output
- **Symptom**: Executing `echo hello | cat` displays nothing and returns immediately to the prompt.
- **Root Cause 1**: `dup2()` called with arguments reversed (e.g., `dup2(STDOUT_FILENO, pipe_fd[1])` instead of `dup2(pipe_fd[1], STDOUT_FILENO)`).
- **Root Cause 2**: `pipe_fd[1]` closed *before* calling `dup2()`, causing `dup2()` to fail with `EBADF` (Bad File Descriptor).
- **Resolution**: Verify call order:
  ```c
  // Correct sequence inside Left Child:
  dup2(pipe_fd[1], STDOUT_FILENO); // 1. Duplicate write end onto STDOUT
  close(pipe_fd[0]);               // 2. Close original read descriptor
  close(pipe_fd[1]);               // 3. Close original write descriptor
  ```

---

### Problem 3: `execvp()` Fails with "No such file or directory"
- **Symptom**: Executing `badcmd | cat` outputs `execvp failed: No such file or directory`.
- **Root Cause**: `left_args[0]` contains an invalid executable name or path that cannot be resolved in system `PATH`.
- **Handling in NexShell**: The child process prints the error using `perror("execvp failed")` and must call `exit(1)`. If the child does not exit on `execvp` failure, it will fall through into the main REPL loop, spawning a duplicate shell process inside a child process.

---

### Problem 4: File Descriptor Leak / Resource Exhaustion
- **Symptom**: After running many piped commands, NexShell starts throwing `pipe failed: Too many open files`.
- **Root Cause**: The parent shell process forgot to close `pipe_fd[0]` and `pipe_fd[1]` after spawning child processes. Open file descriptors accumulate in the parent process table until `EMFILE` (process descriptor limit) is reached.
- **Verification**: Check open descriptors using `/proc/<pid>/fd` or `lsof` (see Section 2).

---

## 2. Diagnostic Tools & Debugging Techniques

> **Note on Platform Availability**: The diagnostic tools described below (`strace`, `/proc`, `lsof`, `gdb`) are natively available on POSIX Linux environments and WSL (Windows Subsystem for Linux).

---

### Method A: Linux Process File Descriptor Inspection (`/proc/<pid>/fd`)

On Linux/WSL, inspect open file descriptors for the running NexShell process in real time:

1. Find the PID of `nexshell`:
   ```bash
   pgrep nexshell
   # Output: 42105
   ```

2. List all open file descriptors:
   ```bash
   ls -l /proc/42105/fd
   ```

3. **Expected Output** for an idle shell (no descriptor leak):
   ```text
   lrwx------ 1 user user 64 Oct 7 17:30 0 -> /dev/pts/1
   lrwx------ 1 user user 64 Oct 7 17:30 1 -> /dev/pts/1
   lrwx------ 1 user user 64 Oct 7 17:30 2 -> /dev/pts/1
   ```
   If descriptors `3`, `4`, `5`, etc. point to `pipe:[XXXXX]`, a descriptor leak exists in the parent shell loop.

---

### Method B: System Call Tracing with `strace`

Trace system call execution, file descriptor redirection, and process creation using `strace`:

```bash
strace -f -e trace=pipe,fork,dup2,close,execvp,waitpid ./nexshell
```

#### Annotated `strace` Output for `ls | sort`:
```text
[pid 45000] pipe([3, 4])                  = 0
[pid 45000] clone(...)                    = 45001 (Left Child)
[pid 45001] dup2(4, 1)                    = 1
[pid 45001] close(3)                      = 0
[pid 45001] close(4)                      = 0
[pid 45001] execvp("ls", ["ls"])          = 0
[pid 45000] clone(...)                    = 45002 (Right Child)
[pid 45002] dup2(3, 0)                    = 0
[pid 45002] close(3)                      = 0
[pid 45002] close(4)                      = 0
[pid 45002] execvp("sort", ["sort"])      = 0
[pid 45000] close(3)                      = 0
[pid 45000] close(4)                      = 0
[pid 45000] waitpid(45001, NULL, 0)       = 45001
[pid 45000] waitpid(45002, NULL, 0)       = 45002
```

---

### Method C: Debugging Child Processes with GDB

By default, GDB follows the parent process on `fork()`. To debug pipeline child processes:

1. Launch GDB:
   ```bash
   gdb ./nexshell
   ```

2. Instruct GDB to follow child processes on `fork()`:
   ```gdb
   (gdb) set follow-fork-mode child
   (gdb) set detach-on-fork off
   (gdb) break main
   (gdb) run
   ```

3. Step through `dup2()` and `execvp()` in the child process:
   ```gdb
   (gdb) next
   (gdb) info proc descriptors
   ```

---

## 3. Quick Troubleshooting Matrix

| Error Message / Symptom | Probable Cause | Corrective Action |
| :--- | :--- | :--- |
| `pipe failed: Too many open files` | Accumulation of unclosed pipe descriptors in parent process loop. | Ensure `close(pipe_fd[0])` and `close(pipe_fd[1])` execute in parent after forking. |
| Shell prompt hangs after pipeline | Parent or child process failed to close write descriptor (`pipe_fd[1]`). | Verify `close(pipe_fd[1])` is invoked in parent, left child, and right child. |
| `dup2 failed: Bad file descriptor` | `close()` was called before `dup2()`, or invalid descriptor passed. | Ensure `dup2()` is executed *before* `close(pipe_fd[X])`. |
| Child falls into REPL loop on error | `exit(1)` missing after `execvp()` failure in child branch. | Add `exit(1)` immediately following `perror("execvp failed")`. |
