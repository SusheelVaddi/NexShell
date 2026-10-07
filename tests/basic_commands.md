# NexShell Test Suite: Basic & Built-in Commands

This document contains test specifications, execution logs, and validation results for NexShell's basic external command execution and parent-process built-in commands.

---

## Test Environment
- **OS**: POSIX / Linux / macOS
- **Compiler**: GCC 14+ / Clang with `-Wall -Wextra`
- **Shell Version**: NexShell 1.0

---

## Test Cases

### Test TC-BASIC-01: Working Directory Inspection (`pwd`)
- **Objective**: Verify that NexShell executes the external `pwd` command, spawns a child process via `fork()`, replaces image with `/bin/pwd` via `execvp()`, and displays the current working directory.
- **Command**:
  ```text
  NexShell> pwd
  ```
- **Expected Behavior**: Prints absolute path of current working directory without trailing junk characters.
- **Actual Behavior**: Prints absolute path (e.g., `/Users/sanjanasamrat/NexShell/NexShell`).
- **Feature Being Verified**: External command execution, process creation (`fork`), program invocation (`execvp`), synchronous waiting (`waitpid`).
- **Status**: **PASS**

---

### Test TC-BASIC-02: Directory Listing (`ls` without arguments)
- **Objective**: Verify that simple external commands with a single argument vector execute and terminate cleanly.
- **Command**:
  ```text
  NexShell> ls
  ```
- **Expected Behavior**: Lists non-hidden files in current directory and returns to `NexShell> ` prompt.
- **Actual Behavior**: Directory contents displayed; prompt displayed immediately after child termination.
- **Feature Being Verified**: Child process execution and argument parsing (`args[0] = "ls"`, `args[1] = NULL`).
- **Status**: **PASS**

---

### Test TC-BASIC-03: Directory Listing with Arguments (`ls -l` / `ls -la`)
- **Objective**: Verify multi-token argument parsing via `strtok()` and passing multiple arguments to `execvp()`.
- **Command**:
  ```text
  NexShell> ls -l
  ```
- **Expected Behavior**: Detailed file listing formatted by system `ls`.
- **Actual Behavior**: Displays permissions, owner, file size, timestamps, and filenames accurately.
- **Feature Being Verified**: Multi-argument tokenization (`MAX_ARGS = 64`).
- **Status**: **PASS**

---

### Test TC-BASIC-04: Directory Creation (`mkdir`)
- **Objective**: Verify external directory creation binary execution.
- **Command**:
  ```text
  NexShell> mkdir demo_test_dir
  ```
- **Expected Behavior**: Creates directory `demo_test_dir` in the working filesystem without errors.
- **Actual Behavior**: Directory created; verified with `ls`.
- **Feature Being Verified**: External command execution with parameter passing.
- **Status**: **PASS**

---

### Test TC-BASIC-05: Directory Traversal (`cd <relative_path>`)
- **Objective**: Verify built-in directory navigation executing strictly within the parent shell process.
- **Command**:
  ```text
  NexShell> cd demo_test_dir
  NexShell> pwd
  ```
- **Expected Behavior**: Changes working directory to `demo_test_dir`; subsequent `pwd` reflects the updated directory.
- **Actual Behavior**: Shell working directory changes in parent process via `chdir()`.
- **Feature Being Verified**: Parent-process built-in execution (`chdir()`).
- **Status**: **PASS**

---

### Test TC-BASIC-06: Parent Directory Traversal (`cd ..`)
- **Objective**: Verify relative upward directory traversal.
- **Command**:
  ```text
  NexShell> cd ..
  NexShell> pwd
  ```
- **Expected Behavior**: Returns to parent directory.
- **Actual Behavior**: Successfully navigated up one directory level.
- **Feature Being Verified**: `chdir("..")` in parent shell.
- **Status**: **PASS**

---

### Test TC-BASIC-07: Default Home Directory Navigation (`cd` with no arguments)
- **Objective**: Verify fallback to `$HOME` environment variable when `cd` is invoked without parameters.
- **Command**:
  ```text
  NexShell> cd
  NexShell> pwd
  ```
- **Expected Behavior**: Navigates to value of `getenv("HOME")`.
- **Actual Behavior**: Successfully navigates to home directory.
- **Feature Being Verified**: `getenv("HOME")` handling within built-in `cd`.
- **Status**: **PASS**

---

### Test TC-BASIC-08: Clean Shell Termination (`exit`)
- **Objective**: Verify clean REPL termination without hanging or abnormal exit codes.
- **Command**:
  ```text
  NexShell> exit
  ```
- **Expected Behavior**: Prints `Exiting NexShell...` and terminates with return code 0.
- **Actual Behavior**: Loop breaks and main returns 0 cleanly.
- **Feature Being Verified**: Built-in `exit` command.
- **Status**: **PASS**

---

### Test TC-BASIC-09: End of File Handling (`Ctrl+D` / `EOF`)
- **Objective**: Verify graceful handling of standard input closure (`fgets` returning `NULL`).
- **Command**:
  ```text
  NexShell> <Ctrl+D>
  ```
- **Expected Behavior**: Prints `\nExiting NexShell...\n` and terminates cleanly.
- **Actual Behavior**: Detects `NULL` from `fgets`, prints exit message, and terminates gracefully.
- **Feature Being Verified**: EOF / stream closure handling.
- **Status**: **PASS**

---

### Test TC-BASIC-10: Whitespace-Only / Empty Input
- **Objective**: Verify shell ignores empty Enter key presses and arbitrary combinations of tabs and spaces.
- **Command**:
  ```text
  NexShell>    
  NexShell> \t \t  
  ```
- **Expected Behavior**: No child processes spawned; redisplays `NexShell> ` prompt cleanly.
- **Actual Behavior**: `trim_whitespace()` produces empty string; loop skips execution and re-prompts.
- **Feature Being Verified**: Whitespace trimming and empty line sanitization.
- **Status**: **PASS**

---

## Summary Matrix

| Test ID | Command | Target Mechanism | Result |
| :--- | :--- | :--- | :--- |
| **TC-BASIC-01** | `pwd` | `fork` + `execvp` | **PASS** |
| **TC-BASIC-02** | `ls` | `fork` + `execvp` | **PASS** |
| **TC-BASIC-03** | `ls -l` | Argument parsing (`strtok`) | **PASS** |
| **TC-BASIC-04** | `mkdir demo_test_dir` | External binary execution | **PASS** |
| **TC-BASIC-05** | `cd demo_test_dir` | Parent process `chdir()` | **PASS** |
| **TC-BASIC-06** | `cd ..` | Parent process `chdir()` | **PASS** |
| **TC-BASIC-07** | `cd` | `getenv("HOME")` | **PASS** |
| **TC-BASIC-08** | `exit` | Built-in exit | **PASS** |
| **TC-BASIC-09** | `EOF (Ctrl+D)` | Stream closure detection | **PASS** |
| **TC-BASIC-10** | `\t   \t` | `trim_whitespace()` | **PASS** |
