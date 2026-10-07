# NexShell - Jury & Technical Presentation Guide

## Overview
This guide provides a structured 3 to 5 minute live presentation script and technical Q&A reference for evaluating **NexShell**.

---

## Live Demonstration Script (3-5 Minutes)

### Step 1: Compilation & REPL Startup (30 Seconds)
**Command:**
```bash
gcc -Wall -Wextra main.c -o nexshell
./nexshell
```
**Demonstrates**: Clean C compilation with zero warnings/errors and interactive REPL initialization with dynamic prompt rendering.

### Step 2: Builtin Commands & Dynamic Prompt (45 Seconds)
**Commands:**
```bash
pwd
mkdir demo_dir
cd demo_dir
pwd
cd ..
rmdir demo_dir
```
**Demonstrates**: Direct process execution within the parent shell using `getcwd()`, `chdir()`, and `mkdir()` system calls without spawning child processes.

### Step 3: Input & Output Redirection (45 Seconds)
**Commands:**
```bash
echo "NexShell Systems Architecture" > demo_out.txt
cat < demo_out.txt
rm -f demo_out.txt
```
**Demonstrates**: File descriptor manipulation using `open()` with flags `O_WRONLY | O_CREAT | O_TRUNC` and `O_RDONLY`, followed by `dup2()` redirection prior to `execvp()`.

### Step 4: Multi-Stage Pipelines (60 Seconds)
**Commands:**
```bash
cat main.c | grep fork | sort | head -n 5
```
**Demonstrates**: Arbitrary N-stage POSIX pipeline implementation using `N-1` pipes, `dup2()` wiring across process boundaries, and descriptor closure discipline.

### Step 5: Background Execution (30 Seconds)
**Commands:**
```bash
sleep 3 &
pwd
```
**Demonstrates**: Asynchronous child execution where the parent shell skips `waitpid()` blocking, prints the child PID, and returns immediately to the REPL. Non-blocking `waitpid(-1, NULL, WNOHANG)` cleans up background zombies upon subsequent REPL iterations.

### Step 6: Signal Handling & Resilience (45 Seconds)
**Commands:**
```bash
sleep 30
# Press Ctrl+C
sleep 30 | cat | cat
# Press Ctrl+C
```
**Demonstrates**: Foreground process signal propagation (`SIGINT` restored to `SIG_DFL` in children), while the parent shell ignores `SIGINT` (`SIG_IGN`) and uses `clearerr(stdin)` to preserve REPL stability.

---

## Technical Concept Explanations

### 1. Process Lifecycle (`fork` / `execvp` / `waitpid`)
- **`fork()`**: Clones the parent shell process into an identical child process.
- **`execvp()`**: Replaces the child process memory space with the target executable image found in `PATH`.
- **`waitpid()`**: Parent shell blocks until the specified foreground child PID changes state (terminates).

### 2. Pipeline Wiring (`pipe` / `dup2`)
- **`pipe(pipefd)`**: Creates a unidirectional data channel returning two file descriptors: `pipefd[0]` (read end) and `pipefd[1]` (write end).
- **`dup2(oldfd, newfd)`**: Duplicates `oldfd` onto standard stream numbers (`0` for stdin, `1` for stdout).
- **Descriptor Closing**: Crucial to close unused pipe descriptors in both parent and child processes so `EOF` is delivered when write ends close.

### 3. I/O Redirection
- **`<`**: Child opens input file in `O_RDONLY` mode and redirects to `STDIN_FILENO` (`dup2(fd, STDIN_FILENO)`).
- **`>`**: Child opens output file in `O_WRONLY | O_CREAT | O_TRUNC` mode (permissions `0644`) and redirects to `STDOUT_FILENO` (`dup2(fd, STDOUT_FILENO)`).

### 4. Background Execution & Zombie Prevention
- When `&` is appended, parent shell prints `[Background process started: PID ...]` and does not invoke blocking `waitpid()`.
- Prior to reading user input, `reap_background_processes()` polls `waitpid(-1, &status, WNOHANG)` to reap terminated background children without blocking REPL interactions.

### 5. Signal Handling (`SIGINT` / `Ctrl+C`)
- **Parent Shell**: Installs `signal(SIGINT, SIG_IGN)` at startup so `Ctrl+C` does not exit NexShell. If `fgets()` is interrupted, `clearerr(stdin)` resets stream flags.
- **Foreground Child**: Resets `signal(SIGINT, SIG_DFL)` immediately after `fork()` so `Ctrl+C` terminates the foreground task.
- **Background Child**: Inherits `SIG_IGN` so background tasks are not terminated by shell prompt `Ctrl+C`.

---

## Likely Jury Questions & Concise Answers

**Q1: Why do you need `dup2()` in child processes?**
> *Answer:* `execvp()` retains standard file descriptors (`0`, `1`, `2`). `dup2()` redirects standard input/output to pipe descriptors or file descriptors prior to `execvp()`, enabling seamless redirection and piping without modifying external programs.

**Q2: How does NexShell prevent zombie processes for background jobs?**
> *Answer:* NexShell calls `waitpid(-1, NULL, WNOHANG)` inside a loop at the start of each REPL iteration to collect exit statuses of any completed background processes non-blockingly.

**Q3: Why must pipe descriptors be closed in both parent and child?**
> *Answer:* If any process retains an open write descriptor (`pipefd[1]`), reading processes waiting on `pipefd[0]` will never receive an EOF signal, causing multi-stage pipelines (like `cat | grep`) to hang indefinitely.

**Q4: How does NexShell handle Ctrl+C without exiting?**
> *Answer:* NexShell sets `SIGINT` to `SIG_IGN` in the parent shell. When `fgets()` returns `NULL` due to signal interrupt, `clearerr(stdin)` clears the stream error, a newline is printed, and the prompt re-renders cleanly.
