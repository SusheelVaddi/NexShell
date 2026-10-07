# NexShell Background Execution Debugging & Troubleshooting Guide

This guide provides troubleshooting protocols, process inspection techniques, and diagnostic steps for background process execution (`&`) in **NexShell**.

---

## 1. Common Issues & Root Cause Analysis

### Problem 1: Accumulation of Zombie Processes (`<defunct>`)
- **Symptom**: Running `ps aux | grep nexshell` displays multiple `<defunct>` process entries.
- **Root Cause**: Terminated child processes are not being reaped by the parent shell process.
- **Resolution**:
  Ensure the non-blocking reaping loop is present at the beginning of the REPL `while (1)` loop in `main.c`:
  ```c
  while (waitpid(-1, NULL, WNOHANG) > 0) {
      // Reap zombie background processes
  }
  ```
  Passing `-1` checks all children, and `WNOHANG` prevents the loop from blocking when no children have exited.

---

### Problem 2: Background Process Prompt Collision
- **Symptom**: Output from a background command appears in the middle of a newly typed user command at the `NexShell> ` prompt.
- **Root Cause**: Background child processes share the same standard output file descriptor (`STDOUT_FILENO` / `/dev/pts/X`) as the parent shell process.
- **Workaround & Solution**:
  This is standard Unix shell behavior when background commands are not explicitly redirected. To suppress output collision, instruct users to redirect stdout to `/dev/null` or a log file:
  ```bash
  NexShell> command > output.log &
  ```

---

### Problem 3: `waitpid()` Returns -1 immediately
- **Symptom**: `waitpid(-1, NULL, WNOHANG)` returns `-1` with `errno` set to `ECHILD` (No child processes).
- **Root Cause**: This is normal behavior when no child processes exist under the parent shell process. The `while` loop condition `> 0` evaluates to `false`, exiting cleanly without error.

---

## 2. Process Inspection Techniques (Linux / WSL)

---

### Technique 1: Inspecting Process States with `ps` and `pstree`

1. **View Process Hierarchy**:
   ```bash
   pstree -p $(pgrep nexshell)
   ```
   *Sample Output*:
   ```text
   nexshell(42100)---sleep(42105)
   ```

2. **Check Process State Codes (`STAT` column)**:
   ```bash
   ps -o pid,ppid,stat,cmd -C nexshell,sleep
   ```
   *State Meanings*:
   - `S`: Interruptible sleep (waiting for event / sleep timer).
   - `R`: Running or runnable on CPU queue.
   - `Z`: **Zombie / Defunct** (terminated, awaiting `waitpid` reaping by parent).

---

### Technique 2: Process Memory & Status Inspection (`/proc/<pid>/status`)

Examine the detailed state of a background child process via the Linux proc filesystem:

```bash
cat /proc/<pid>/status | head -n 10
```

*Sample Output*:
```text
Name:   sleep
Umask:  0022
State:  S (sleeping)
Tgid:   45310
Ngid:   0
Pid:    45310
PPid:   42100
FdSize: 64
```
Verify that `PPid` matches the Process ID of the running `nexshell` instance.

---

### Technique 3: Debugging Background Spawning in GDB

To debug parent/child interactions during background execution:

1. Launch GDB:
   ```bash
   gdb ./nexshell
   ```

2. Set GDB to remain in parent process on `fork()`:
   ```gdb
   (gdb) set follow-fork-mode parent
   (gdb) break main.c:373
   (gdb) run
   ```

3. Enter `sleep 5 &` at the prompt.
4. Inspect `is_background` flag and `pid` variable:
   ```gdb
   (gdb) print is_background
   $1 = 1
   (gdb) print pid
   $2 = 45350
   ```

---

## 3. Quick Reference Troubleshooting Table

| Symptom | Probable Cause | Diagnostic Command | Fix / Mitigation |
| :--- | :--- | :--- | :--- |
| Accumulation of `<defunct>` processes | Missing `waitpid(..., WNOHANG)` loop in parent. | `ps aux \| grep defunct` | Ensure `while(waitpid(-1, NULL, WNOHANG) > 0)` loop runs every prompt iteration. |
| Background process hangs on user input | Background child attempting to read from `STDIN`. | `cat /proc/<pid>/status` | Redirect background input using `< /dev/null` or input files. |
| Text corruption on prompt | Background process writing to terminal stdout. | Terminal inspection | Redirect output using `> logfile.txt &`. |
