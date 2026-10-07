# NexShell 3–5 Minute Hackathon & Jury Demonstration Guide

This guide provides a structured, timed presentation script for demonstrating **NexShell** to project evaluators, professors, or hackathon judges.

---

## 1. Demo Objectives & Elevator Pitch (30 Seconds)
- **Objective**: Demonstrate that NexShell is a fully functional, lightweight Unix-like shell written in C that demonstrates low-level POSIX systems programming concepts without external dependencies.
- **Key Message to Jury**: *"NexShell implements process lifecycle management (`fork`, `execvp`, `waitpid`), inter-process communication (`pipe`), file descriptor duplication (`dup2`), and asynchronous task execution (`&`) with automatic zombie reclamation (`WNOHANG`)."*

---

## 2. Environment Setup (15 Seconds)
Open a clean Linux or WSL terminal and compile with diagnostic flags:
```bash
gcc -Wall -Wextra main.c -o nexshell
./nexshell
```

---

## 3. Live Demonstration Sequence (3 Minutes)

### Step 1: Basic Navigation & Process Execution
```text
NexShell> pwd
```
- **Expected Output**: Displays current working directory path.
- **Talking Point**: *"We execute standard external system binaries by forking a child process and calling `execvp()`, while the parent synchronizes using `waitpid()`."*

```text
NexShell> ls
```
- **Expected Output**: Lists directory entries.
- **Talking Point**: *"The shell tokenizes arguments dynamically using `strtok()` up to `MAX_ARGS = 64`."*

---

### Step 2: Directory Creation & Parent-Process Built-ins
```text
NexShell> mkdir jury_demo
NexShell> cd jury_demo
NexShell> pwd
```
- **Expected Output**: Directory created; working directory reflects `/jury_demo`.
- **Talking Point**: *"Notice `cd` executes in the parent process using `chdir()`. If it were executed in a child process, the parent shell's working directory would remain unchanged upon child exit."*

```text
NexShell> cd ..
```
- **Expected Output**: Navigates back up one directory level.

---

### Step 3: Input & Output Redirection
```text
NexShell> ls > output.txt
```
- **Expected Output**: No screen output; file `output.txt` created with directory listing.
- **Talking Point**: *"Here the child process opens `output.txt` with `O_WRONLY | O_CREAT | O_TRUNC` and duplicates it onto `STDOUT_FILENO` (descriptor 1) via `dup2()`."*

```text
NexShell> cat < output.txt
```
- **Expected Output**: Contents of `output.txt` printed to terminal.
- **Talking Point**: *"Now `cat` receives its standard input from the file by duplicating the descriptor onto `STDIN_FILENO` (descriptor 0)."*

---

### Step 4: Inter-Process Communication Pipelines
```text
NexShell> ls | sort
```
- **Expected Output**: Alphabetically sorted directory listing.
- **Talking Point**: *"NexShell creates a unidirectional kernel data channel with `pipe()`. The left child binds stdout to the pipe write end (`pipe_fd[1]`), while the right child binds stdin to the read end (`pipe_fd[0]`). Both ends are closed cleanly in all processes to prevent descriptor leaks."*

---

### Step 5: Asynchronous Background Execution & Zombie Reaping
```text
NexShell> sleep 10 &
```
- **Expected Output**: `[Background process started: PID <pid>]` with immediate prompt return.
- **Talking Point**: *"Appending `&` launches the task asynchronously without blocking the parent shell. Notice the prompt returns immediately."*

```text
NexShell> pwd
```
- **Expected Output**: Working directory prints instantly while `sleep` runs in background.
- **Talking Point**: *"To prevent zombie processes, NexShell calls `waitpid(-1, NULL, WNOHANG)` at the start of every REPL loop, automatically cleaning up terminated background children."*

---

### Step 6: Graceful Shell Exit
```text
NexShell> exit
```
- **Expected Output**: `Exiting NexShell...` and returns status 0.

---

## 4. Backup / Alternative Test Sequence
If judges ask for edge-case or error handling demonstrations:

```text
# 1. Non-existent directory handling:
NexShell> cd /fake_dir_12345
cd failed: No such file or directory

# 2. Syntax validation on redirection:
NexShell> ls >
Error: Missing output filename.

# 3. Pipeline syntax validation:
NexShell> | grep test
Error: Missing command before '|'.

# 4. Stream string piping:
NexShell> echo "NexShell Rocks" | cat
NexShell Rocks
```
