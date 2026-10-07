# NexShell Test Suite: Error Handling & Edge Cases

This document details test scenarios validating NexShell's resilience, error reporting, input boundary limits, and edge case handling.

---

## Test Cases

### Test TC-ERR-01: Non-Existent Command
- **Objective**: Verify error handling when user enters an executable name not present in system `PATH`.
- **Command**:
  ```text
  NexShell> non_existent_command_xyz123
  ```
- **Expected Behavior**: Displays `execvp failed: No such file or directory`; child exits with status 1; parent shell remains responsive.
- **Actual Behavior**: Error printed via `perror("execvp failed")`; shell prompts for next command.
- **Feature Being Verified**: `execvp()` failure handling and parent error recovery.
- **Status**: **PASS**

---

### Test TC-ERR-02: Non-Existent Directory in `cd`
- **Objective**: Verify built-in `cd` reports system errors cleanly when navigating to an invalid directory.
- **Command**:
  ```text
  NexShell> cd /path/to/non_existent_directory_9999
  ```
- **Expected Behavior**: Displays `cd failed: No such file or directory`; shell working directory remains unchanged.
- **Actual Behavior**: `chdir()` returns `-1`; `perror("cd failed")` invoked; working directory preserved.
- **Feature Being Verified**: Built-in `cd` error handling.
- **Status**: **PASS**

---

### Test TC-ERR-03: Repeated Output Redirection Operators (`>>`)
- **Objective**: Verify rejection of unsupported append redirection.
- **Command**:
  ```text
  NexShell> ls >> out.txt
  ```
- **Expected Behavior**: Displays `Error: Multiple or repeated '>' redirection operators.`.
- **Actual Behavior**: Detected by operator count check; rejected before process fork.
- **Feature Being Verified**: Unsupported operator validation.
- **Status**: **PASS**

---

### Test TC-ERR-04: Repeated Input Redirection Operators (`<<`)
- **Objective**: Verify rejection of unsupported heredoc redirection.
- **Command**:
  ```text
  NexShell> cat << in.txt
  ```
- **Expected Behavior**: Displays `Error: Multiple or repeated '<' redirection operators.`.
- **Actual Behavior**: Detected by operator count check; rejected before process fork.
- **Feature Being Verified**: Unsupported operator validation.
- **Status**: **PASS**

---

### Test TC-ERR-05: Missing Output Filename
- **Objective**: Verify syntax checking when output redirection has no target.
- **Command**:
  ```text
  NexShell> ls >
  ```
- **Expected Behavior**: Displays `Error: Missing output filename.`.
- **Actual Behavior**: String length check on target file catches empty argument; emits error.
- **Feature Being Verified**: Redirection argument validation.
- **Status**: **PASS**

---

### Test TC-ERR-06: Missing Input Filename
- **Objective**: Verify syntax checking when input redirection has no target.
- **Command**:
  ```text
  NexShell> cat <
  ```
- **Expected Behavior**: Displays `Error: Missing input filename.`.
- **Actual Behavior**: String length check on target file catches empty argument; emits error.
- **Feature Being Verified**: Redirection argument validation.
- **Status**: **PASS**

---

### Test TC-ERR-07: Missing Command in Pipeline
- **Objective**: Verify error handling for leading or trailing pipe characters.
- **Commands**:
  ```text
  NexShell> | grep test
  NexShell> ls |
  ```
- **Expected Behavior**: Displays `Error: Missing command before '|'.` / `Error: Missing command after '|'.`.
- **Actual Behavior**: Verified; syntax errors reported without process forks.
- **Feature Being Verified**: Pipeline syntax validation.
- **Status**: **PASS**

---

### Test TC-ERR-08: Buffer Boundary Limits
- **Objective**: Verify behavior with maximum command line input (up to 1024 characters) and maximum argument count (up to 64 tokens).
- **Command**:
  ```text
  NexShell> echo arg1 arg2 arg3 ... arg63
  ```
- **Expected Behavior**: Successfully tokenizes up to 63 arguments plus trailing `NULL`, executing without memory corruption.
- **Actual Behavior**: `parse_command()` respects `MAX_ARGS - 1` boundary and `args[count] = NULL`.
- **Feature Being Verified**: Buffer safety and bounds checking (`MAX_INPUT_SIZE`, `MAX_ARGS`).
- **Status**: **PASS**

---

## Summary Matrix

| Test ID | Command | Target Mechanism | Result |
| :--- | :--- | :--- | :--- |
| **TC-ERR-01** | `non_existent_command` | `execvp` failure handling | **PASS** |
| **TC-ERR-02** | `cd /invalid/path` | `chdir` error handling | **PASS** |
| **TC-ERR-03** | `ls >> file.txt` | Repeated operator validation | **PASS** |
| **TC-ERR-04** | `cat << file.txt` | Repeated operator validation | **PASS** |
| **TC-ERR-05** | `ls >` | Missing output file validation | **PASS** |
| **TC-ERR-06** | `cat <` | Missing input file validation | **PASS** |
| **TC-ERR-07** | `\| cmd` / `cmd \|` | Pipeline command validation | **PASS** |
| **TC-ERR-08** | Long argument vectors | Buffer limit enforcement | **PASS** |
