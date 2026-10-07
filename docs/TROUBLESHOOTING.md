# NexShell Troubleshooting & Diagnostic Guide

This guide provides practical troubleshooting steps, root cause analyses, solutions, and diagnostic commands for common issues encountered when building, running, testing, or contributing to **NexShell**.

---

## 1. Build & Compiler Issues

### 1.1 Problem: `gcc: command not found`
- **Cause**: The GCC compiler or build-essential package is not installed on the system.
- **Solution**: Install the compiler toolchain using the system package manager:
  - **Ubuntu / Debian / WSL**: `sudo apt update && sudo apt install -y build-essential gcc`
  - **Fedora / Red Hat**: `sudo dnf install gcc`
  - **macOS**: `xcode-select --install`
- **Example Diagnostic**:
  ```bash
  gcc --version
  ```

---

### 1.2 Problem: Compilation Warnings or Errors
- **Cause**: Incompatible header definitions, missing standard library packages, or unhandled compiler flags.
- **Solution**: Always compile with standard diagnostic flags:
  ```bash
  gcc -Wall -Wextra main.c -o nexshell
  ```
- **Example Diagnostic**:
  ```bash
  # Check if all POSIX headers resolve correctly
  gcc -E main.c > /dev/null
  ```

---

## 2. Runtime & Execution Issues

### 2.3 Problem: `Permission denied` when running `./nexshell`
- **Cause**: The compiled binary does not have execute permissions.
- **Solution**: Grant execute permissions to the binary:
  ```bash
  chmod +x nexshell
  ./nexshell
  ```

---

### 2.4 Problem: `execvp failed: No such file or directory`
- **Cause**: The requested command does not exist in standard system `PATH` directories (`/bin`, `/usr/bin`, `/usr/local/bin`).
- **Solution**: Verify the binary name or provide the full path to the executable.
- **Example**:
  ```text
  NexShell> non_existent_tool
  execvp failed: No such file or directory
  ```

---

### 2.5 Problem: `cd failed: No such file or directory`
- **Cause**: The target directory path provided to `cd` does not exist or has incorrect permissions.
- **Solution**: Verify directory existence with `ls` or `pwd` before navigating.
- **Example**:
  ```text
  NexShell> cd /tmp/invalid_folder
  cd failed: No such file or directory
  ```

---

## 3. Redirection & Piping Issues

### 3.6 Problem: `open failed: No such file or directory` on `<`
- **Cause**: The source file for input redirection does not exist in the working directory.
- **Solution**: Ensure the input file exists before redirecting it into a command.
- **Example**:
  ```text
  NexShell> cat < missing_input.txt
  open failed: No such file or directory
  ```

---

### 3.7 Problem: `Error: Missing output filename.` or `Error: Missing input filename.`
- **Cause**: A redirection operator (`>` or `<`) was entered without a destination or source filename.
- **Solution**: Provide a valid file path immediately following the operator.
- **Example**:
  ```text
  # Incorrect:
  NexShell> ls >
  
  # Correct:
  NexShell> ls > filelist.txt
  ```

---

### 3.8 Problem: `Error: Multiple or repeated '>' redirection operators.`
- **Cause**: NexShell supports single-operator redirection (`>`) but does not implement append mode (`>>`) or multiple output redirections.
- **Solution**: Use single `>` redirection.
- **Example**:
  ```text
  # Incorrect:
  NexShell> ls >> out.txt
  
  # Correct:
  NexShell> ls > out.txt
  ```

---

### 3.9 Problem: `Error: Missing command before '|'.` or `after '|'.`
- **Cause**: The pipe operator `|` was entered without valid subcommands on both sides.
- **Solution**: Ensure both the producing command and consuming command are specified.
- **Example**:
  ```text
  # Incorrect:
  NexShell> | grep main
  NexShell> ls |
  
  # Correct:
  NexShell> ls | grep main
  ```

---

## 4. Background Execution & Process Lifecycle

### 4.10 Problem: Background Process Output Mixes with Prompt
- **Cause**: Background processes inherit the parent's `stdout` unless redirected. When a background task writes to standard output, its text appears on the terminal while the parent shell prompt is active.
- **Solution**: Redirect background command output to a file if you do not want terminal interference.
- **Example**:
  ```text
  NexShell> (sleep 2; echo "Done") > bg_log.txt &
  ```

---

### 4.11 Problem: Zombie Processes Accumulating
- **Cause**: Terminated background child processes remain in the process table until reaped by the parent.
- **Solution**: NexShell automatically invokes `waitpid(-1, NULL, WNOHANG)` on every prompt cycle. Pressing `Enter` or running any command triggers immediate cleanup.
- **Diagnostic Command**:
  ```bash
  ps -el | grep defunct
  ```

---

## 5. Environment & Git Authentication

### 5.12 Problem: WSL Carriage Return (`\r`) / Line Ending Errors
- **Cause**: Editing source files on Windows with CRLF line endings can corrupt string parsing on Linux.
- **Solution**: Convert file line endings to Unix LF:
  ```bash
  dos2unix main.c
  ```

---

### 5.13 Problem: GitHub Push Returns HTTP 403 Forbidden
- **Cause**: macOS Keychain or Git credential cache holds stale authentication tokens for GitHub.
- **Solution**: Clear the cached credential and use a Personal Access Token (PAT):
  ```bash
  printf "protocol=https\nhost=github.com\n\n" | git credential-osxkeychain erase
  git push origin main
  ```
