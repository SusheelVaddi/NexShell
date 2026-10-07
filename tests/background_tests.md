# NexShell Test Suite: Background Execution & Process Management

This document specifies test scenarios and validation logs for NexShell's asynchronous background execution operator (`&`) and non-blocking zombie process cleanup (`waitpid` with `WNOHANG`).

---

## Test Cases

### Test TC-BG-01: Asynchronous Command Spawning (`sleep <seconds> &`)
- **Objective**: Verify that appending `&` executes the command in the background without blocking the parent shell.
- **Command**:
  ```text
  NexShell> sleep 2 &
  ```
- **Expected Behavior**: Displays `[Background process started: PID <pid>]` and immediately outputs the `NexShell> ` prompt.
- **Actual Behavior**: Child process PID printed; prompt returned instantly; parent process accepts subsequent commands while sleep runs.
- **Feature Being Verified**: Asynchronous process creation, background flag setting, prompt non-blocking behavior.
- **Status**: **PASS**

---

### Test TC-BG-02: Immediate Command Execution During Background Task
- **Objective**: Verify that the parent shell remains fully interactive while background child executes.
- **Command**:
  ```text
  NexShell> sleep 5 &
  NexShell> pwd
  ```
- **Expected Behavior**: `pwd` executes immediately and prints the working directory while `sleep 5` continues executing in the background.
- **Actual Behavior**: `pwd` executed and completed instantly without waiting for `sleep 5`.
- **Feature Being Verified**: Concurrent background and foreground process scheduling.
- **Status**: **PASS**

---

### Test TC-BG-03: Non-Blocking Zombie Process Cleanup (`WNOHANG`)
- **Objective**: Verify that terminated background child processes are reaped to prevent zombie processes in the OS process table.
- **Command**:
  ```text
  NexShell> sleep 1 &
  # (Wait 2 seconds)
  NexShell> pwd
  ```
- **Expected Behavior**: Child process terminates; loop start executes `while (waitpid(-1, NULL, WNOHANG) > 0)` and reaps the dead process without blocking.
- **Actual Behavior**: Process table inspected; child is cleaned up and does not remain in `<defunct>` zombie state.
- **Feature Being Verified**: Non-blocking zombie process reaping (`WNOHANG`).
- **Status**: **PASS**

---

### Test TC-BG-04: Background Pipeline Execution (`cmd1 | cmd2 &`)
- **Objective**: Verify background execution when combined with command piping.
- **Command**:
  ```text
  NexShell> sleep 1 | cat &
  ```
- **Expected Behavior**: Displays `[Background process started: PIDs <pid1>, <pid2>]` and returns prompt immediately.
- **Actual Behavior**: Both child PIDs reported; parent does not block on `waitpid()`.
- **Feature Being Verified**: Asynchronous pipeline execution.
- **Status**: **PASS**

---

### Test TC-BG-05: Background Redirection Execution (`cmd > file &`)
- **Objective**: Verify background execution when combined with output redirection.
- **Command**:
  ```text
  NexShell> ls > bg_out.txt &
  ```
- **Expected Behavior**: Displays `[Background process started: PID <pid>]`, writes output to `bg_out.txt`, and returns prompt immediately.
- **Actual Behavior**: Output file written in background; prompt returned without delay.
- **Feature Being Verified**: Background redirection execution.
- **Status**: **PASS**

---

### Test TC-BG-06: Missing Command Before Background Operator (`&`)
- **Objective**: Verify error detection when `&` is entered alone or preceded only by whitespace.
- **Command**:
  ```text
  NexShell> &
  ```
- **Expected Behavior**: Displays `Error: Missing command before '&'.` and does not spawn any process.
- **Actual Behavior**: Error message printed; prompt returns.
- **Feature Being Verified**: Background syntax validation.
- **Status**: **PASS**

---

## Summary Matrix

| Test ID | Command | Target Mechanism | Result |
| :--- | :--- | :--- | :--- |
| **TC-BG-01** | `sleep 2 &` | Asynchronous child fork & PID report | **PASS** |
| **TC-BG-02** | `sleep 5 &` + `pwd` | Parent responsiveness during background run | **PASS** |
| **TC-BG-03** | `sleep 1 &` + reaping | `waitpid(-1, NULL, WNOHANG)` zombie cleanup | **PASS** |
| **TC-BG-04** | `sleep 1 \| cat &` | Asynchronous pipeline execution | **PASS** |
| **TC-BG-05** | `ls > bg_out.txt &` | Background I/O redirection | **PASS** |
| **TC-BG-06** | `&` | Missing command validation | **PASS** |
