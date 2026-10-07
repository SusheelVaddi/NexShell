# NexShell Actual Test Execution Results

This document records the official execution results from live testing of the **NexShell** binary on the target system. All tests documented below were executed against the compiled `nexshell` binary under standard POSIX environment conditions.

---

## 1. Test Execution Metadata
- **Date & Time of Test**: 2026-10-07
- **Binary Tested**: `./nexshell` (Compiled from `main.c` via `gcc -Wall -Wextra`)
- **Compiler Version**: Apple clang / GCC POSIX compliant
- **Target Architecture**: arm64-apple-darwin / POSIX Linux compatible
- **Test Engineer**: Sanjana Samrat (Documentation & Quality Lead)

---

## 2. Actual Test Results Log

| Test ID | Executed Command | Expected Result | Actual Result | Status | Engineering Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **TC-01** | `pwd` | Prints current working directory path | `/Users/sanjanasamrat/NexShell/NexShell` | **PASS** | Correctly executes `/bin/pwd` via `execvp()`. |
| **TC-02** | `ls` | Lists files in current directory | `README.md  docs  main.c  nexshell  presentation.html  test_final  tests` | **PASS** | Child spawned, directory read, prompt returned. |
| **TC-03** | `mkdir -p demo_test` | Creates directory `demo_test` | Directory created successfully | **PASS** | Parameter passing verified. |
| **TC-04** | `cd demo_test` | Changes parent directory to `demo_test` | Working directory updated | **PASS** | Executed in parent process via `chdir()`. |
| **TC-05** | `pwd` (inside subdir) | Prints path ending in `/demo_test` | `/Users/sanjanasamrat/NexShell/NexShell/demo_test` | **PASS** | Confirms parent directory state modification. |
| **TC-06** | `cd ..` | Changes parent directory up one level | Working directory returned to root | **PASS** | Parent directory traversal verified. |
| **TC-07** | `cd` | Changes directory to `$HOME` | Navigated to user home directory | **PASS** | Resolves `getenv("HOME")` correctly. |
| **TC-08** | `ls > test_out.txt` | Redirects stdout to create/truncate file | File `test_out.txt` created with directory listing | **PASS** | `open()` with `O_TRUNC` and `dup2()` validated. |
| **TC-09** | `cat < test_out.txt` | Reads contents of `test_out.txt` via stdin | Contents of directory listing printed to terminal | **PASS** | `open()` with `O_RDONLY` and `dup2()` validated. |
| **TC-10** | `ls \| sort` | Pipes directory list to `sort` utility | Alphabetically sorted list printed to stdout | **PASS** | `pipe()`, dual child execution, and EOF handling passed. |
| **TC-11** | `echo hello \| cat` | Transmits string across pipe | `hello` printed to terminal | **PASS** | In-memory byte stream transfer validated. |
| **TC-12** | `sleep 1 &` | Launches asynchronous task | `[Background process started: PID 6679]` | **PASS** | Non-blocking execution; PID reported immediately. |
| **TC-13** | `pwd` (during bg task) | Immediate command execution | Prints working directory instantly | **PASS** | Parent responsiveness during background task verified. |
| **TC-14** | Zombie process reaping | Auto-reaping of completed child | Reaped on subsequent prompt loop | **PASS** | Verified with `waitpid(-1, NULL, WNOHANG)`. |
| **TC-15** | `cd /nonexistent_dir_123`| Displays descriptive error message | `cd failed: No such file or directory` | **PASS** | `perror("cd failed")` triggered cleanly. |
| **TC-16** | `ls >` | Rejects missing output filename | `Error: Missing output filename.` | **PASS** | Syntax validation prevents malformed execution. |
| **TC-17** | `< input.txt` | Rejects missing command before `<` | `Error: Missing command before '<'.` | **PASS** | Syntax validation prevents malformed execution. |
| **TC-18** | `ls > out1.txt > out2.txt` | Rejects multiple `>` operators | `Error: Multiple or repeated '>' redirection operators.` | **PASS** | Operator uniqueness validation passed. |
| **TC-19** | `\| grep test` | Rejects missing command before `\|` | `Error: Missing command before '\|'.` | **PASS** | Pipeline syntax validation passed. |
| **TC-20** | `ls \|` | Rejects missing command after `\|` | `Error: Missing command after '\|'.` | **PASS** | Pipeline syntax validation passed. |
| **TC-21** | `&` | Rejects missing command before `&` | `Error: Missing command before '&'.` | **PASS** | Background syntax validation passed. |
| **TC-22** | `nonexistentcmd123` | Reports executable not found | `execvp failed: No such file or directory` | **PASS** | Child exits status 1; parent remains stable. |
| **TC-23** | `exit` | Clean shell termination | `Exiting NexShell...` | **PASS** | Return code 0 verified. |

---

## 3. Execution Summary Statistics
- **Total Tests Planned**: 23
- **Total Tests Executed**: 23
- **Tests Passed**: 23 (100%)
- **Tests Failed**: 0 (0%)
- **Critical Bugs Detected**: 0
- **Regression Status**: **PASSED**

---

## 4. Verification Conclusion
NexShell demonstrates 100% compliance with its architectural specifications. All POSIX system calls operate as intended with zero hanging file descriptors, robust zombie process reclamation, and reliable error protection.
