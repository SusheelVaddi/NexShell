# NexShell Complete Test Plan & Quality Assurance Strategy

This document establishes the official Quality Assurance and Verification Strategy for the **NexShell** project. It outlines the methodologies, testing scope, test cases, execution procedures, and acceptance criteria required to validate the shell's reliability and compliance with POSIX standards.

---

## 1. Testing Objectives
The primary objectives of the NexShell test plan are:
1. **Functional Correctness**: Ensure all built-in commands (`cd`, `exit`) and supported operators (`>`, `<`, `|`, `&`) behave in strict compliance with expected POSIX shell semantics.
2. **Process Lifecycle Safety**: Verify proper process spawning (`fork`), image replacement (`execvp`), parent-child synchronization (`waitpid`), and complete prevention of zombie processes or file descriptor leaks.
3. **Robust Error Handling**: Ensure invalid inputs, non-existent files, syntax errors, and missing binaries fail gracefully with descriptive error messages without crashing the shell.
4. **Boundary & Stress Resistance**: Validate buffer safety under boundary constraints (`MAX_INPUT_SIZE = 1024`, `MAX_ARGS = 64`).

---

## 2. Test Environment
Testing must be performed on standard Unix-like operating systems:
- **Operating Systems**: Linux (Ubuntu 22.04 / 24.04 LTS, Debian, Fedora) or WSL (Windows Subsystem for Linux 2).
- **Compilers**: GCC 11+ or Clang with strict diagnostic flags (`-Wall -Wextra -pedantic`).
- **Standard Library**: GNU C Library (`glibc`) / POSIX.1-2008 compliant API.

---

## 3. Compilation & Build Verification
The build process must compile cleanly without producing any compiler warnings or errors.

| Step | Command | Expected Output | Status |
| :--- | :--- | :--- | :--- |
| **Strict Compilation** | `gcc -Wall -Wextra main.c -o nexshell` | Clean compilation (0 errors, 0 warnings) | **VERIFIED** |
| **Executable Permissions** | `chmod +x nexshell` | Read/Write/Execute bits set on binary | **VERIFIED** |

---

## 4. Test Matrix & Detailed Verification Scenarios

### 4.1 Basic & Built-in Command Testing

| Test Category | Command | Target System Call | Expected Behavior | Acceptance Standard |
| :--- | :--- | :--- | :--- | :--- |
| **Working Directory** | `pwd` | `fork()`, `execvp()`, `waitpid()` | Prints absolute directory path | Exact match with system `pwd` |
| **Directory Listing** | `ls` | `fork()`, `execvp()` | Lists files in current directory | Displays directory items cleanly |
| **Argument Passing** | `ls -l` | Argument array tokenization | Detailed formatted file list | Correct parameter passing |
| **Directory Navigation** | `cd <dir>` | `chdir()` in parent | Working directory updated | Subsequent `pwd` reflects new dir |
| **Parent Traversal** | `cd ..` | `chdir("..")` in parent | Traverses up one directory level | Directory path updated |
| **Home Traversal** | `cd` | `getenv("HOME")`, `chdir()` | Navigates to user home directory | Navigates to `$HOME` |
| **Directory Creation** | `mkdir test_dir` | `fork()`, `execvp()` | Creates directory in filesystem | Directory exists and verifiable |
| **Shell Exit** | `exit` | Built-in loop break | Prints `Exiting NexShell...` & returns 0 | Zero exit status |
| **Stream Termination** | `Ctrl+D` (`EOF`) | `fgets()` returns `NULL` | Detects EOF, prints exit message, exits | Clean exit without loop hang |
| **Empty Input** | `[Enter]` / `\t   ` | `trim_whitespace()` | Ignores whitespace, re-prompts | No process spawned |

---

### 4.2 I/O Redirection Testing

| Category | Command | Flags & Modes | Expected Result |
| :--- | :--- | :--- | :--- |
| **Output Redirection** | `ls > output.txt` | `O_WRONLY \| O_CREAT \| O_TRUNC` (0644) | Writes directory list to `output.txt` |
| **Output Overwrite** | `echo "New" > output.txt` | `O_TRUNC` | Truncates prior content and replaces with new |
| **Input Redirection** | `cat < output.txt` | `O_RDONLY` | Reads standard input from `output.txt` |
| **Tight Syntax Output** | `ls>output.txt` | Lexical parsing | Correctly identifies `>` without spaces |
| **Tight Syntax Input** | `cat<output.txt` | Lexical parsing | Correctly identifies `<` without spaces |
| **Dual Redirection (1)** | `cat < input.txt > output.txt` | Dual `dup2()` (0 and 1) | Reads input file and writes to output file |
| **Dual Redirection (2)** | `cat > output.txt < input.txt` | Dual `dup2()` (1 and 0) | Handles inverted operator ordering |

---

### 4.3 Command Piping Testing

| Category | Command | POSIX Mechanism | Expected Result |
| :--- | :--- | :--- | :--- |
| **Single Pipeline** | `ls \| grep main` | `pipe()`, `dup2()`, dual `fork()` | Filters directory list to matching lines |
| **Data Stream Pipe** | `echo hello \| cat` | In-memory pipe buffer | Transmits data without file creation |
| **Pipeline with Args** | `ls -l \| grep doc` | Dual `parse_command()` vectors | Correct multi-token execution on both ends |
| **Pipeline Termination** | `ls \| sort` | Pipe EOF on write-end close | `sort` reads EOF, terminates cleanly |
| **Tight Pipe Syntax** | `ls\|grep main` | Pipe lexical splitter | Executes identical to spaced pipe |

---

### 4.4 Background Execution & Process Reaping

| Category | Command | Target Mechanism | Expected Result |
| :--- | :--- | :--- | :--- |
| **Async Execution** | `sleep 2 &` | Background flag, PID print | Prints `[Background process started: PID <pid>]`, returns prompt immediately |
| **Parent Responsiveness**| `sleep 5 &` then `pwd` | Concurrent scheduling | `pwd` executes immediately without waiting |
| **Zombie Process Reaping**| `sleep 1 &` + loop cycle | `waitpid(-1, NULL, WNOHANG)` | Reaps terminated child; prevents zombie `<defunct>` state |
| **Background Pipeline** | `sleep 1 \| cat &` | Dual async PID tracking | Prints both child PIDs; returns prompt |
| **Background Redirection**| `ls > bg.txt &` | Async descriptor redirection | Completes file write asynchronously |

---

### 4.5 Error Handling & Boundary Verification

| Test Scenario | Input Command | Expected Error Output | Handling Action |
| :--- | :--- | :--- | :--- |
| **Invalid Binary** | `nonexistent_cmd` | `execvp failed: No such file or directory` | Child exits with 1; parent re-prompts |
| **Invalid Directory** | `cd /invalid_dir_999` | `cd failed: No such file or directory` | `perror()` reports error; directory preserved |
| **Repeated `>` Operator** | `ls >> file.txt` | `Error: Multiple or repeated '>' redirection operators.` | Input rejected before fork |
| **Repeated `<` Operator** | `cat << file.txt` | `Error: Multiple or repeated '<' redirection operators.` | Input rejected before fork |
| **Missing Output File** | `ls >` | `Error: Missing output filename.` | Input rejected; no process created |
| **Missing Input File** | `cat <` | `Error: Missing input filename.` | Input rejected; no process created |
| **Non-Existent Input File**| `cat < missing.txt` | `open failed: No such file or directory` | Child reports error, exits with 1 |
| **Missing Left Pipe Cmd** | `\| grep test` | `Error: Missing command before '\|'.` | Input rejected; no process created |
| **Missing Right Pipe Cmd**| `ls \|` | `Error: Missing command after '\|'.` | Input rejected; no process created |
| **Missing Background Cmd**| `&` | `Error: Missing command before '&'.` | Input rejected; no process created |

---

## 5. Regression & Integration Testing
Before any release or demonstration, the full integration test script (`tests/integration_tests.md`) must be executed sequentially to ensure no regression in multi-command session workflows.

---

## 6. Final Acceptance Criteria
A build is certified as **Ready for Release/Demo** only when:
1. `gcc -Wall -Wextra main.c -o nexshell` produces **0 warnings and 0 errors**.
2. All 10 basic command test cases (**TC-BASIC-01** to **TC-BASIC-10**) pass.
3. All 12 redirection test cases (**TC-REDIR-01** to **TC-REDIR-12**) pass.
4. All 9 pipe test cases (**TC-PIPE-01** to **TC-PIPE-09**) pass.
5. All 6 background execution test cases (**TC-BG-01** to **TC-BG-06**) pass.
6. All 8 error handling test cases (**TC-ERR-01** to **TC-ERR-08**) pass.
7. Memory and descriptor leak checks verify that all unused pipe ends and file descriptors are closed in both child and parent contexts.
