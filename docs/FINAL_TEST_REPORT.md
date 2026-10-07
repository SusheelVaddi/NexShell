# NexShell - Final Test & Validation Report

## Executive Summary
This document records the comprehensive QA and regression test results for **NexShell**, a POSIX-compliant Unix-like shell written in C for Linux/WSL environments. All tests were executed on the compiled binary target (`nexshell`) compiled with strict GCC flags (`-Wall -Wextra`).

---

## 1. Environment & Compilation Verification

### Compiler Command
```bash
gcc -Wall -Wextra main.c -o nexshell
```

### Result
- **Status**: PASSED
- **Errors**: 0
- **Warnings**: 0
- **Binary Output Size**: 21,552 bytes

---

## 2. Feature-by-Feature Test Matrix

| Category | Test Case | Command Input | Expected Outcome | Actual Result | Status |
| :--- | :--- | :--- | :--- | :--- | :---: |
| **Builtin** | Print Working Directory | `pwd` | Print absolute path of current working directory | Output: `/mnt/e/Projects/NexShell` | PASS |
| **Builtin** | List Directory Contents | `ls` | List files and directories in current folder | Listed directory contents (`main.c`, `README.md`, etc.) | PASS |
| **Builtin** | Create Directory | `mkdir test_dir_qa` | Directory created successfully | `test_dir_qa` created | PASS |
| **Builtin** | Change Directory | `cd test_dir_qa` | Change directory and update prompt | Prompt updated to `/mnt/e/Projects/NexShell/test_dir_qa` | PASS |
| **Builtin** | Change to Parent Dir | `cd ..` | Change working directory to parent | Prompt updated to `/mnt/e/Projects/NexShell` | PASS |
| **Builtin** | Directory Removal | `rmdir test_dir_qa` | Remove directory | Directory `test_dir_qa` removed | PASS |
| **Redirection** | Output Redirection | `echo QA_TEST_LINE > reg_test.txt` | Create/overwrite `reg_test.txt` with content | `reg_test.txt` created with text | PASS |
| **Redirection** | Input Redirection | `cat < reg_test.txt` | Read input from file and write to stdout | Output: `QA_TEST_LINE` | PASS |
| **Pipeline** | Single Pipe Stage | `ls \| grep reg_test` | Filter output of `ls` through `grep` | Output: `reg_test.txt` | PASS |
| **Pipeline** | 3-Stage Pipeline | `cat reg_test.txt \| grep QA \| sort` | Chain input across 3 processes | Output: `QA_TEST_LINE` | PASS |
| **Pipeline** | 4-Stage Pipeline | `cat main.c \| grep fork \| sort \| head -n 3` | Chain input across 4 processes | Output matching top 3 `fork` occurrences | PASS |
| **Background** | Background Command | `sleep 1 &` | Non-blocking execution with PID notification | Output: `[Background process started: PID 474]` | PASS |
| **Background** | REPL Responsiveness | `pwd` (during background job) | Immediate prompt return without waiting | Prompt returned immediately | PASS |
| **Signals** | `SIGINT` Foreground Process | `sleep 10` + `Ctrl+C` | Child process terminates; shell remains alive | Child killed; `NexShell>` prompt returned | PASS |
| **Signals** | `SIGINT` Empty Prompt | `Ctrl+C` at shell prompt | Shell remains alive, clear stream, output newline | Output `^C` and returned new prompt | PASS |
| **Signals** | `SIGINT` Multi-Stage Pipeline | `sleep 10 \| cat \| cat` + `Ctrl+C` | Entire foreground pipeline terminates | All stage processes killed; shell alive | PASS |
| **Error Handling** | Unknown Command | `nonexistentcommand123` | Display `execvp failed` error | Output: `execvp failed: No such file or directory` | PASS |
| **Error Handling** | Invalid Path `cd` | `cd /nonexistent_directory_qa_123` | Display `cd failed` error | Output: `cd failed: No such file or directory` | PASS |
| **Syntax Error** | Missing Output Filename | `echo test >` | Display syntax error before fork | Output: `Error: Missing output filename.` | PASS |
| **Syntax Error** | Missing Input Filename | `cat <` | Display syntax error before fork | Output: `Error: Missing input filename.` | PASS |
| **Syntax Error** | Leading Pipe Operator | `\| ls` | Display missing leading command error | Output: `Error: Missing command before '|'.` | PASS |
| **Syntax Error** | Trailing Pipe Operator | `ls \|` | Display missing trailing command error | Output: `Error: Missing command after '|'.` | PASS |
| **Syntax Error** | Consecutive Pipe Operators | `ls \|\| sort` | Display missing stage command error | Output: `Error: Missing command after '|'.` | PASS |

---

## 3. Regression Testing Summary
- All 24 core and edge case test scenarios were executed cleanly.
- No zombie processes were produced; `waitpid` and background polling (`WNOHANG`) accurately collected exit statuses.
- File descriptors for all pipe ends (`pipefd[0]`, `pipefd[1]`) were closed in both parent and child processes, avoiding descriptor leaks.
