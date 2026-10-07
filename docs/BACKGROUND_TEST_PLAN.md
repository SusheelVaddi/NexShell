# NexShell Background Execution Test Plan & Matrix

This document presents a comprehensive test plan and verification matrix for background execution (`&`) in **NexShell**.

---

## 1. Test Methodology

Background execution tests evaluate asynchronous child spawning, PID reporting, non-blocking prompt availability, zombie process prevention via `WNOHANG`, and edge-case syntax parsing.

---

## 2. Comprehensive Test Matrix

### Test Category A: Standard Asynchronous Execution

#### Test ID: `BG-01`
- **Purpose**: Verify standard background command execution and prompt availability.
- **Command**: `sleep 2 &`
- **Expected Result**: Displays `[Background process started: PID <pid>]` and immediately redisplays `NexShell> ` prompt.
- **Actual Result**: `[Background process started: PID 45310]` displayed immediately. Terminal prompt accepts new input while `sleep 2` executes.
- **Pass/Fail**: PASS
- **Notes**: Verifies `is_background` flag, `fork()`, PID printing, and skipping of blocking `waitpid()`.

#### Test ID: `BG-02`
- **Purpose**: Verify simultaneous execution of multiple concurrent background processes.
- **Command**: `sleep 5 &` followed immediately by `sleep 3 &`
- **Expected Result**: Displays separate PIDs for both background jobs (`[Background process started: PID <pid1>]`, `[Background process started: PID <pid2>]`). Both run concurrently.
- **Actual Result**: Two distinct PIDs reported. Both processes execute in background without interference.
- **Pass/Fail**: PASS
- **Notes**: Validates process table isolation for multiple concurrent child processes.

#### Test ID: `BG-03`
- **Purpose**: Verify interactive prompt responsiveness immediately after launching a background job.
- **Command**: `sleep 2 &` followed immediately by `pwd`
- **Expected Result**: `sleep 2 &` prints PID. `pwd` prints current working directory immediately without waiting for `sleep 2`.
- **Actual Result**:
  ```text
  NexShell> sleep 2 &
  [Background process started: PID 45320]
  NexShell> pwd
  /home/user/NexShell
  ```
- **Pass/Fail**: PASS
- **Notes**: Confirms prompt responsiveness and immediate foreground execution while background job is running.

#### Test ID: `BG-04`
- **Purpose**: Verify execution and automatic reaping of short-lived background processes.
- **Command**: `echo hello &`
- **Expected Result**: Displays background PID, outputs `hello` to stdout, and reaps child process.
- **Actual Result**: Background PID printed, `hello` displayed, prompt remains responsive.
- **Pass/Fail**: PASS
- **Notes**: Validates handling of rapid child termination and non-blocking cleanup.

#### Test ID: `BG-05`
- **Purpose**: Verify long-running background process stability.
- **Command**: `sleep 10 &`
- **Expected Result**: Background PID displayed. Shell remains fully operational for 10 seconds. Process reaped on subsequent prompt loop once completed.
- **Actual Result**: PID printed, shell continues accepting commands. Process reaped cleanly after 10s.
- **Pass/Fail**: PASS
- **Notes**: Verifies asynchronous lifecycle management over extended time intervals.

---

### Test Category B: Error Handling & Syntax Edge Cases

#### Test ID: `BG-06`
- **Purpose**: Verify handling of invalid/non-existent background commands.
- **Command**: `invalidcmd &`
- **Expected Result**: Displays `[Background process started: PID <pid>]`. Child outputs `execvp failed: No such file or directory` and exits (`exit(1)`).
- **Actual Result**: PID printed. Error `execvp failed: No such file or directory` outputted by child. Parent shell prompt remains active.
- **Pass/Fail**: PASS
- **Notes**: Confirms child error isolation: `execvp` failure in background child does not crash parent shell.

#### Test ID: `BG-07`
- **Purpose**: Verify parsing of background operator with trailing spaces/tabs.
- **Command**: `sleep 2 &   `
- **Expected Result**: Correctly identifies `&`, strips trailing spaces, sets `is_background = 1`, and spawns `sleep 2`.
- **Actual Result**: PID printed, `sleep 2` executed in background.
- **Pass/Fail**: PASS
- **Notes**: Validates trailing whitespace loop in background symbol parser (`main.c` line 71).

#### Test ID: `BG-08`
- **Purpose**: Verify handling of standalone `&` operator without a command.
- **Command**: `&`
- **Expected Result**: Display error `Error: Missing command before '&'.` and re-display prompt.
- **Actual Result**: `Error: Missing command before '&'.` printed to stdout. No child forked.
- **Pass/Fail**: PASS
- **Notes**: Confirms empty command string check when `is_background == 1` (`main.c` line 86).

#### Test ID: `BG-09`
- **Purpose**: Verify parsing of multiple `&` characters in input.
- **Command**: `sleep 2 &&` or `ls & &`
- **Expected Result**: Outer `&` stripped, inner `&` passed to command string or handled gracefully.
- **Actual Result**: `strrchr` finds last `&`, strips it, and passes remaining string (`sleep 2 &` or `ls &`) to command tokenizer.
- **Pass/Fail**: PASS
- **Notes**: Tests `strrchr()` position matching behavior.

---

### Test Category C: Zombie Reaping & Regression Verification

#### Test ID: `BG-10`
- **Purpose**: Verify zombie process cleanup using non-blocking `waitpid(-1, NULL, WNOHANG)`.
- **Command**: Run `sleep 1 &`, wait 2 seconds, then press ENTER at `NexShell> ` prompt.
- **Expected Result**: `waitpid(-1, NULL, WNOHANG)` reaps the exited background child. Checking `ps aux` reveals zero zombie processes.
- **Actual Result**: Process reaped on prompt cycle. `ps` shows no defunct process entries.
- **Pass/Fail**: PASS
- **Notes**: Verifies non-blocking reaping loop at start of REPL iteration (`main.c` line 47).

#### Test ID: `BG-11`
- **Purpose**: Verify regression stability of standard foreground commands after background execution.
- **Command**: `ls -l` (executed after launching background jobs)
- **Expected Result**: Standard directory listing printed cleanly in foreground.
- **Actual Result**: Normal directory listing outputted. Shell state fully preserved.
- **Pass/Fail**: PASS
- **Notes**: Confirms that background process state (`is_background`) resets to 0 for subsequent foreground inputs.

---

## 3. Test Results Summary

| Category | Total Tests | Passed | Failed | Status |
| :--- | :--- | :--- | :--- | :--- |
| **A. Standard Asynchronous Execution** | 5 | 5 | 0 | 100% Pass |
| **B. Error & Syntax Edge Cases** | 4 | 4 | 0 | 100% Pass |
| **C. Zombie Reaping & Regression** | 2 | 2 | 0 | 100% Pass |
| **TOTAL** | **11** | **11** | **0** | **100% Pass** |
