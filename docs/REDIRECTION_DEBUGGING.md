# NexShell Redirection Debugging & Troubleshooting Guide

This guide provides an engineering reference for diagnosing, debugging, and resolving issues in NexShell's Input and Output Redirection subsystem. It details root causes for common error messages, step-by-step diagnostic procedures, GDB debugging techniques, and Linux kernel inspection tools.

---

## 1. Quick Diagnostic Flowchart

When diagnosing a redirection failure, follow this evaluation sequence:

```
Command entered: e.g. "cat < in.txt > out.txt"
                     │
                     ▼
       Did shell display an error message?
        ┌────────────┴────────────┐
        │ YES                     │ NO
        ▼                         ▼
Is it an "Error: ..." message?   Did the command produce expected output?
  ├─ YES: Pre-fork validation failed.      ├─ YES: Working correctly.
  │  (Check syntax, operators, filenames)  └─ NO: Check execution failure
  │                                               (See Section 3)
  └─ NO: System call error from perror().
     (Check open(), dup2(), execvp() in Section 2)
```

---

## 2. Common System Call Errors & Root Causes

### 2.1 `open failed: No such file or directory` (ENOENT)
- **Manifestation**:
  ```text
  NexShell> cat < missing_file.txt
  open failed: No such file or directory
  ```
- **Root Cause**:
  1. The target file specified for input redirection (`<`) does not exist in the specified path.
  2. For output redirection (`>`), an intermediate directory component in the path does not exist (e.g., `ls > /nonexistent_dir/output.txt`). While `O_CREAT` creates missing regular files, it cannot create non-existent parent directories.
- **Resolution**:
  - Verify file existence using `ls -l <filename>`.
  - Check whether relative paths resolve against the shell's current working directory using `pwd`.
  - For output files in subdirectories, ensure parent folders exist using `mkdir -p`.

### 2.2 `open failed: Permission denied` (EACCES)
- **Manifestation**:
  ```text
  NexShell> cat < protected_file.txt
  open failed: Permission denied
  ```
- **Root Cause**:
  1. For input redirection: The current user does not have read permissions (`r`) on the file.
  2. For output redirection: The destination file exists but lacks write permissions (`w`), or the enclosing directory lacks write/execute permissions for the user.
- **Resolution**:
  - Check permissions with `ls -l <filename>`.
  - Modify permissions if appropriate using `chmod u+r <filename>` or `chmod u+w <filename>`.

### 2.3 `dup2 failed: Bad file descriptor` (EBADF)
- **Manifestation**:
  ```text
  dup2 failed: Bad file descriptor
  ```
- **Root Cause**:
  - The source descriptor (`input_fd` or `output_fd`) passed to `dup2()` is invalid (e.g., negative or already closed).
- **Resolution**:
  - Confirm `open()` succeeded (`fd >= 0`) before invoking `dup2()`.
  - Ensure `close(fd)` is NOT called prior to `dup2(fd, STDOUT_FILENO)`.

### 2.4 `execvp failed: No such file or directory` (ENOENT)
- **Manifestation**:
  ```text
  NexShell> badcommand > output.txt
  execvp failed: No such file or directory
  ```
- **Root Cause**:
  - The redirection succeeded (the file was created/truncated), but the command binary itself was not found in any directory listed in the environment `$PATH`.
- **Resolution**:
  - Note that `perror("execvp failed")` prints to standard error (`stderr`, descriptor 2), which remains directed to the terminal screen even when standard output (`stdout`, descriptor 1) is redirected into a file.
  - Check binary spelling and verify executable location with `which <command>`.

---

## 3. Symptom-Based Troubleshooting

### Symptom 1: Output File is Created but Stays 0 Bytes (Empty)
- **Probable Causes**:
  1. The executed command wrote diagnostic information to `stderr` rather than `stdout`. (NexShell redirects descriptor 1, not descriptor 2).
  2. The command failed during execution (e.g., invalid command arguments), so nothing was written to `stdout`.
  3. The command produced no output naturally (e.g., `touch`, `mkdir`, `sleep`).
- **Diagnosis**:
  Run the command without redirection in standard terminal:
  ```bash
  NexShell> ls -z
  ls: invalid option -- 'z'
  ```
  Notice that error messages go to `stderr`. Running `ls -z > out.txt` writes the error to the terminal while `out.txt` remains 0 bytes.

### Symptom 2: Output File Unexpectedly Overwritten
- **Probable Cause**:
  - By POSIX specification, `>` opens files with `O_TRUNC`. If you run two successive commands targeting the same output file:
    ```bash
    NexShell> echo first > out.txt
    NexShell> echo second > out.txt
    ```
    The second command resets the file size to 0 bytes before writing, replacing the first line entirely.
- **Resolution**:
  - This is expected Unix shell behavior. To preserve previous contents in an educational shell, use separate output files or concatenate via temporary files.

### Symptom 3: Shell Hangs (Command Waits Indefinitely)
- **Probable Cause**:
  - The command expected input from standard input, but input redirection was not configured or the target file was empty.
  - If a user runs `cat > out.txt` without `< in.txt`, `cat` is waiting for keyboard input from the user.
- **Resolution**:
  - Provide input by typing text and pressing `Ctrl+D` (EOF), or supply an input file using `cat < input.txt > out.txt`.

### Symptom 4: Terminal Prompt Disappears After Running Redirection
- **Probable Cause**:
  - Stream redirection was executed in the parent process rather than the child process. If `dup2()` overwrites descriptor 1 in the parent shell, all subsequent `printf("NexShell> ")` calls write into the file on disk instead of the terminal display.
- **Resolution**:
  - Verify that `dup2()` is strictly enclosed inside `else if (pid == 0) { ... }` in `main.c`.

---

## 4. Inspecting File Descriptors with Linux Tooling

### 4.1 Inspecting Active Descriptors via `/proc`
On Linux, every active process exposes its open file descriptors under `/proc/<PID>/fd/`.

To inspect open descriptors of the running shell:
```bash
# In another terminal window:
ls -l /proc/$(pgrep nexshell)/fd/
```
Expected baseline output for an idle NexShell session:
```text
lrwx------ 1 user user 64 Oct  7 17:00 0 -> /dev/pts/1  (Keyboard stdin)
lrwx------ 1 user user 64 Oct  7 17:00 1 -> /dev/pts/1  (Terminal stdout)
lrwx------ 1 user user 64 Oct  7 17:00 2 -> /dev/pts/1  (Terminal stderr)
```
If you see lingering entries pointing to files (e.g., `3 -> /home/user/output.txt`), an open descriptor was leaked and not closed via `close()`.

### 4.2 Tracing System Calls with `strace`
`strace` intercepts and records all kernel system calls made by NexShell and its child processes:

```bash
strace -f -e trace=open,openat,dup2,close,execve,fork,wait4,waitpid ./nexshell
```

**Annotated Output for `ls > output.txt`**:
```text
[pid 10240] clone(...) = 10241               <-- Parent forks child
[pid 10241] openat(AT_FDCWD, "output.txt", O_WRONLY|O_CREAT|O_TRUNC, 0644) = 3  <-- Child opens file on FD 3
[pid 10241] dup2(3, 1) = 1                    <-- Child duplicates FD 3 to STDOUT (FD 1)
[pid 10241] close(3) = 0                      <-- Child closes temporary FD 3
[pid 10241] execve("/bin/ls", ["ls"], ...)    <-- Child executes binary
[pid 10240] wait4(10241, NULL, 0, NULL) = 10241 <-- Parent reaps child
```

Verify that:
1. `openat()` returns a descriptor `3`.
2. `dup2(3, 1)` returns `1`.
3. `close(3)` is called immediately before `execve()`.

---

## 5. Debugging NexShell with GDB

Because NexShell forks child processes to handle redirection, debugging requires instructing GDB to follow child processes.

### 5.1 Setting Up GDB for Child Process Inspection
```bash
gcc -g -Wall -Wextra main.c -o nexshell
gdb ./nexshell
```

Inside GDB:
```gdb
# Instruct GDB to follow child processes after fork():
(gdb) set follow-fork-mode child

# Break inside child redirection handler:
(gdb) break execute_child_redirection

# Start the shell:
(gdb) run
NexShell> ls > output.txt

# Execution stops at breakpoint inside child:
Breakpoint 1, execute_child_redirection (redir=0x7fffffffdf00, args=0x7fffffffdd80) at main.c:143
```

### 5.2 Useful GDB Commands for Redirection Inspection
```gdb
# Print parsed redirection metadata:
(gdb) print *redir
$1 = {
  cmd_part = 0x7fffffffe000 "ls",
  input_file = 0x0,
  output_file = 0x7fffffffe005 "output.txt",
  has_input_redirect = 0,
  has_output_redirect = 1
}

# Step through open() and check returned file descriptor:
(gdb) next
(gdb) print output_fd
$2 = 3

# Step through dup2() and verify return value:
(gdb) next
(gdb) print dup2(output_fd, 1)
$3 = 1

# Check argument array passed to execvp:
(gdb) print args[0]
$4 = 0x7fffffffe000 "ls"
(gdb) print args[1]
$5 = 0x0
```

---

## 6. Developer Checklist for New Redirection Features

Before submitting changes to the redirection subsystem:
- [ ] Are all opened file descriptors closed with `close()` immediately after `dup2()`?
- [ ] If `dup2()` fails, is the original descriptor closed before `exit(1)`?
- [ ] Does `open()` check for negative return values (`< 0`) and report via `perror()`?
- [ ] Are invalid operator syntax patterns (`>>`, `<<`, empty filenames) validated **before** `fork()`?
- [ ] Does `execvp()` receive clean argument arrays without operator strings or filenames?
- [ ] Does the parent shell survive invalid files or non-existent commands without crashing?
- [ ] Does `gcc -Wall -Wextra main.c -o nexshell` compile with 0 warnings?
