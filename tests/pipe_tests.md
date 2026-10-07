# NexShell Test Suite: Command Piping

This document specifies test scenarios and validation logs for NexShell's inter-process communication pipeline (`|`), verifying pipe allocation, child process creation, file descriptor duplication, and stream data flow.

---

## Test Cases

### Test TC-PIPE-01: Basic Command Pipe (`ls | grep <pattern>`)
- **Objective**: Verify standard output of the left child (`ls`) is channeled into the standard input of the right child (`grep`).
- **Command**:
  ```text
  NexShell> ls | grep main
  ```
- **Expected Behavior**: Outputs only matching directory entries (e.g., `main.c`).
- **Actual Behavior**: Left child writes directory list to `pipe_fd[1]`; right child reads from `pipe_fd[0]` and filters lines containing "main"; output displayed.
- **Feature Being Verified**: `pipe()`, dual `fork()`, `dup2(pipe_fd[1], 1)`, `dup2(pipe_fd[0], 0)`, concurrent execution.
- **Status**: **PASS**

---

### Test TC-PIPE-02: String Piping (`echo hello | cat`)
- **Objective**: Verify data transmission through pipe without filesystem intermediaries.
- **Command**:
  ```text
  NexShell> echo hello | cat
  ```
- **Expected Behavior**: Prints `hello` to standard output.
- **Actual Behavior**: Message passed cleanly across pipe buffer into `cat`.
- **Feature Being Verified**: In-memory byte stream transfer via POSIX pipe.
- **Status**: **PASS**

---

### Test TC-PIPE-03: Multi-Argument Pipeline (`ls -l | grep docs`)
- **Objective**: Verify that both left and right commands in a pipeline support multi-word arguments.
- **Command**:
  ```text
  NexShell> ls -l | grep docs
  ```
- **Expected Behavior**: Parses `ls -l` for left command and `grep docs` for right command; prints matching formatted row.
- **Actual Behavior**: Tokenized argument arrays `left_args` and `right_args` passed correctly to separate `execvp()` calls.
- **Feature Being Verified**: Argument tokenization inside pipeline execution.
- **Status**: **PASS**

---

### Test TC-PIPE-04: Pipeline Sorting (`ls | sort`)
- **Objective**: Verify pipeline termination when right process consumes entire stream before outputting.
- **Command**:
  ```text
  NexShell> ls | sort
  ```
- **Expected Behavior**: Alphabetically sorted directory list.
- **Actual Behavior**: `sort` reads EOF upon left child termination (because parent closed write end `pipe_fd[1]`), sorts data, and prints.
- **Feature Being Verified**: Proper pipe descriptor closure preventing hanging read blocks on EOF.
- **Status**: **PASS**

---

### Test TC-PIPE-05: Tight Pipe Syntax (`ls|grep main`)
- **Objective**: Verify pipe tokenization when no whitespace separates the pipe operator from command names.
- **Command**:
  ```text
  NexShell> ls|grep main
  ```
- **Expected Behavior**: Successfully parses left command as `ls` and right command as `grep main`.
- **Actual Behavior**: `strchr(..., '|')` splits string; `trim_whitespace()` cleans both tokens; execution succeeds.
- **Feature Being Verified**: Whitespace-insensitive pipe tokenization.
- **Status**: **PASS**

---

### Test TC-PIPE-06: Missing Left Command (`| grep test`)
- **Objective**: Verify error detection when pipe operator lacks a preceding command.
- **Command**:
  ```text
  NexShell> | grep test
  ```
- **Expected Behavior**: Displays `Error: Missing command before '|'.` and does not fork processes.
- **Actual Behavior**: Validates `strlen(left_cmd_part) == 0` and emits error.
- **Feature Being Verified**: Pipeline syntax validation.
- **Status**: **PASS**

---

### Test TC-PIPE-07: Missing Right Command (`ls |`)
- **Objective**: Verify error detection when pipe operator lacks a succeeding command.
- **Command**:
  ```text
  NexShell> ls |
  ```
- **Expected Behavior**: Displays `Error: Missing command after '|'.` and skips execution.
- **Actual Behavior**: Validates `strlen(right_cmd_part) == 0` and emits error.
- **Feature Being Verified**: Pipeline syntax validation.
- **Status**: **PASS**

---

### Test TC-PIPE-08: Invalid Left Command (`nonexistent_cmd | cat`)
- **Objective**: Verify shell resilience when the producing process fails to execute.
- **Command**:
  ```text
  NexShell> nonexistent_cmd | cat
  ```
- **Expected Behavior**: Left child reports `execvp failed: No such file or directory` and exits; right child receives EOF and exits; parent remains responsive.
- **Actual Behavior**: Error printed; parent shell re-prompts normally.
- **Feature Being Verified**: Pipeline child error isolation.
- **Status**: **PASS**

---

### Test TC-PIPE-09: Invalid Right Command (`ls | nonexistent_cmd`)
- **Objective**: Verify shell resilience when the consuming process fails to execute.
- **Command**:
  ```text
  NexShell> ls | nonexistent_cmd
  ```
- **Expected Behavior**: Right child reports `execvp failed: No such file or directory`; shell parent recovers gracefully.
- **Actual Behavior**: Child exit handled by `waitpid()`; shell remains fully operational.
- **Feature Being Verified**: Pipeline consumer error isolation.
- **Status**: **PASS**

---

## Summary Matrix

| Test ID | Command | Target Mechanism | Result |
| :--- | :--- | :--- | :--- |
| **TC-PIPE-01** | `ls \| grep main` | POSIX `pipe()` + dual child execution | **PASS** |
| **TC-PIPE-02** | `echo hello \| cat` | In-memory stream transmission | **PASS** |
| **TC-PIPE-03** | `ls -l \| grep docs` | Multi-argument pipeline parsing | **PASS** |
| **TC-PIPE-04** | `ls \| sort` | EOF handling on pipe closure | **PASS** |
| **TC-PIPE-05** | `ls\|grep main` | Tight syntax parsing | **PASS** |
| **TC-PIPE-06** | `\| grep test` | Missing left command validation | **PASS** |
| **TC-PIPE-07** | `ls \|` | Missing right command validation | **PASS** |
| **TC-PIPE-08** | `nonexistent \| cat` | Producer error resilience | **PASS** |
| **TC-PIPE-09** | `ls \| nonexistent` | Consumer error resilience | **PASS** |
