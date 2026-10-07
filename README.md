# NexShell — A Simplified Unix-like Shell in C

## 1. Project Overview
**NexShell** is an educational, lightweight Unix-like command-line interpreter implemented in C for Linux and WSL (Windows Subsystem for Linux) environments. It provides a customized interactive Read-Eval-Print Loop (REPL) interface that reads user commands, tokenizes command-line arguments, and directly invokes native POSIX operating system calls to manage processes and input/output streams.

The core objective of NexShell is to provide a clean, dependency-free reference implementation illustrating how modern Unix-like operating systems handle process creation, inter-process communication (IPC), file descriptor manipulation, and asynchronous task execution.

---

## 2. Problem Statement
Command-line shells serve as the primary interface between human users and the operating system kernel. Developing a functional shell requires solving several low-level systems programming challenges:
- **Process Lifecycle Management**: Dynamically creating child processes, isolating execution contexts, and preventing orphaned or zombie processes.
- **Inter-Process Communication (IPC)**: Setting up unidirectional byte streams between processes running concurrently.
- **I/O Redirection**: Manipulating standard input (`stdin`) and standard output (`stdout`) file descriptors at the kernel level without affecting parent shell state.
- **Parent Process Environment**: Handling state-modifying operations (such as directory traversal) within the parent shell process rather than a spawned child.

NexShell addresses these challenges by implementing an efficient REPL loop that directly interacts with the Linux kernel via POSIX system calls.

---

## 3. Features
- **Interactive REPL Prompt**: Continuous prompt display (`NexShell> `) with robust input handling and graceful exit on End-of-File (`Ctrl+D` / `EOF`).
- **Built-in Commands**:
  - `exit`: Cleanly terminates the interactive shell session.
  - `cd [dir]`: Changes the current working directory in the parent shell process via `chdir()`. If no argument is passed, defaults to `$HOME`.
- **External Command Execution**: Executes standard system binaries (e.g., `pwd`, `ls`, `mkdir`, `cat`, `grep`, `sleep`, `sort`, `echo`) by spawning child processes using `fork()`, `execvp()`, and `waitpid()`.
- **Output Redirection (`>`)**: Redirects standard output (`stdout`) to write or truncate a destination file, with support for standard (`cmd > file`) and tight (`cmd>file`) syntax.
- **Input Redirection (`<`)**: Redirects standard input (`stdin`) to read data directly from a file, with support for standard (`cmd < file`) and tight (`cmd<file`) syntax.
- **Dual Redirection (`<` and `>`)**: Supports simultaneous input and output redirection on a single command line (e.g., `cmd < input.txt > output.txt`, `cmd > output.txt < input.txt`, and `cmd<input.txt>output.txt`).
- **Defensive Redirection Validation**: Validates syntax before forking; rejects repeated operators (`>>`, `<<`), missing filenames, and missing commands with clear diagnostics.
- **Multi-Stage Command Piping (`|`)**: Inter-process communication connecting standard output of each command stage to standard input of the subsequent stage across multi-command pipelines (e.g., `cmd1 | cmd2 | cmd3`) using $N-1$ POSIX `pipe()` channels and $N$ child processes.
- **Background Execution (`&`)**: Spawns commands asynchronously without blocking the parent shell prompt, immediately reporting child process IDs (PIDs).
- **Non-blocking Zombie Process Reaping**: Automatically reaps terminated background child processes before every prompt cycle using `waitpid(-1, NULL, WNOHANG)`.

---

## 4. Architecture / Working Flow
NexShell follows a structured execution pipeline for every command entered:

```
+-----------------------------------------------------------------------+
| 1. Input Loop: Display prompt "NexShell> ", read line via fgets()     |
+-----------------------------------------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
| 2. Zombie Reaping: Invoke waitpid(-1, NULL, WNOHANG) to clear zombies |
+-----------------------------------------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
| 3. Parse & Trim: Strip newline, identify '&', trim leading/trailing   |
|    whitespace, tokenize arguments (max 64 args, 1024 char buffer)     |
+-----------------------------------------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
| 4. Operator Interception & Dispatch:                                  |
|    - Built-ins ('exit', 'cd') -> Execute directly in parent process   |
|    - Pipeline ('|')          -> Create pipe(), fork 2 children, dup2 |
|    - Redirection ('>', '<')  -> parse_redirection(), fork, dup2 FDs   |
|    - General Commands        -> Fork child, execvp()                  |
+-----------------------------------------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
| 5. Process Synchronization:                                           |
|    - Foreground: Parent invokes waitpid() until child terminates      |
|    - Background ('&'): Parent prints PID and immediately returns      |
+-----------------------------------------------------------------------+
```

### Detailed Component Logic:
1. **Input Trimming & Tokenization**: Raw user input is sanitized using `trim_whitespace()` to eliminate extraneous spaces/tabs. Commands are tokenized with `strtok()` using space and tab delimiters.
2. **Built-in Handling**: `cd` is executed directly in the parent shell process via `chdir()`. Spawning a child process for `cd` would only change the child's working directory, leaving the parent unchanged.
3. **Pipeline Flow**: When `|` is detected, the command is split into left and right subcommands. A POSIX pipe (`pipe_fd[2]`) is allocated. Two child processes are forked:
   - Left Child: `dup2(pipe_fd[1], STDOUT_FILENO)` routes standard output to the pipe write-end.
   - Right Child: `dup2(pipe_fd[0], STDIN_FILENO)` routes standard input from the pipe read-end.
   - Both ends of the pipe are closed in both children and the parent process to prevent hanging descriptor leaks.
4. **Redirection Flow**: When `>` or `<` is detected, syntax is validated and parsed into a `RedirectionInfo` struct by `parse_redirection()`. The child process executes `execute_child_redirection()` to open files with `open()` (`O_WRONLY | O_CREAT | O_TRUNC` for output; `O_RDONLY` for input) and redirects standard descriptors via `dup2()`.

---

## 5. System Calls Used

| System Call | Header File | Purpose in NexShell |
| :--- | :--- | :--- |
| **`fork()`** | `<unistd.h>` | Clones the parent shell process to create a child process with an isolated execution context. |
| **`execvp()`** | `<unistd.h>` | Replaces the child process image with the specified executable binary by searching system `PATH`. |
| **`waitpid()`** | `<sys/wait.h>` | Synchronizes parent with child (blocking foreground wait or non-blocking zombie reaping with `WNOHANG`). |
| **`pipe()`** | `<unistd.h>` | Allocates a unidirectional IPC data channel (`pipe_fd[0]` read, `pipe_fd[1]` write). |
| **`dup2()`** | `<unistd.h>` | Duplicates open file descriptors onto `STDIN_FILENO` (0) or `STDOUT_FILENO` (1) for redirection and pipes. |
| **`open()`** | `<fcntl.h>` | Opens or creates target files for input reading (`O_RDONLY`) or output writing (`O_WRONLY \| O_CREAT \| O_TRUNC`). |
| **`close()`** | `<unistd.h>` | Closes file descriptors and unused pipe ends to avoid descriptor and resource leaks. |
| **`chdir()`** | `<unistd.h>` | Modifies the current working directory of the parent shell process. |

---

## 6. Build and Run Instructions

### Prerequisites
- **Operating System**: Linux (Ubuntu, Debian, Fedora, Arch) or WSL (Windows Subsystem for Linux).
- **Compiler**: GCC (GNU Compiler Collection) or Clang.
- **Build Utilities**: Standard C library (`libc`) and POSIX header files.

### Compilation
Compile the source code using `gcc` with standard compiler warnings enabled:
```bash
gcc -Wall main.c -o nexshell
```

### Execution
Launch the compiled NexShell binary:
```bash
./nexshell
```

---

## 7. Demo Commands

The following command sequences illustrate standard interaction patterns supported by NexShell:

```text
NexShell> pwd
/home/user/NexShell

NexShell> ls
main.c  nexshell  presentation.html  README.md

NexShell> mkdir demo_dir
NexShell> cd demo_dir
NexShell> pwd
/home/user/NexShell/demo_dir

NexShell> cd ..
NexShell> ls > files.txt
NexShell> cat < files.txt
main.c
nexshell
presentation.html
README.md
files.txt

NexShell> ls | grep main
main.c

NexShell> sleep 5 &
[Background process started: PID 45210]

NexShell> pwd
/home/user/NexShell

NexShell> exit
Exiting NexShell...
```

---

## 8. Testing and Validation

NexShell has been systematically validated across core Unix command execution scenarios and edge cases. Below is the test matrix detailing each verified operation:

### 1. Working Directory Inspection (`pwd`)
- **Command**:
  ```text
  NexShell> pwd
  ```
- **Expected Behavior**: Prints the absolute directory path of the current shell session.
- **Verification**: Spawns a child process with `fork()`, executes `/bin/pwd` via `execvp()`, and blocks until child termination with `waitpid()`.

### 2. Directory Listing (`ls`)
- **Command**:
  ```text
  NexShell> ls -l
  ```
- **Expected Behavior**: Lists directory entries, permissions, and file details formatted by the standard `ls` utility.
- **Verification**: Tests argument tokenization (`strtok()`) and array passing (`char *args[]`) to `execvp()`.

### 3. Directory Navigation (`cd`)
- **Commands**:
  ```text
  NexShell> cd demo_dir
  NexShell> cd ..
  NexShell> cd
  ```
- **Expected Behavior**:
  - `cd <dir>`: Changes the current working directory to `<dir>`.
  - `cd ..`: Traverses to the parent directory.
  - `cd`: Traverses to the user's home directory (resolving `$HOME`).
  - Invalid directory: Displays error message `cd failed: No such file or directory`.
- **Verification**: Confirms execution strictly in the parent shell process via `chdir()`.

### 4. Directory Creation (`mkdir`)
- **Command**:
  ```text
  NexShell> mkdir test_folder
  ```
- **Expected Behavior**: Creates the specified directory within the current working folder.
- **Verification**: Verifies multi-argument external command execution (`mkdir` + folder name).

### 5. Output Redirection (`>`)
- **Command**:
  ```text
  NexShell> ls > output.txt
  ```
- **Expected Behavior**: Executes `ls` without printing to the terminal screen; creates or truncates `output.txt` and writes directory listing into it.
- **Verification**: Validates `open(output.txt, O_WRONLY | O_CREAT | O_TRUNC, 0644)` followed by descriptor replacement via `dup2(fd, STDOUT_FILENO)`.

### 6. Input Redirection (`<`)
- **Command**:
  ```text
  NexShell> cat < output.txt
  ```
- **Expected Behavior**: Reads file contents from `output.txt` via redirected standard input and prints the content to the terminal.
- **Verification**: Validates `open(output.txt, O_RDONLY)` followed by `dup2(fd, STDIN_FILENO)`.

### 7. Command Piping (`|`)
- **Command**:
  ```text
  NexShell> ls | grep main
  ```
- **Expected Behavior**: Connects stdout of `ls` to stdin of `grep main`, outputting only entries matching `main` (e.g., `main.c`).
- **Verification**: Validates `pipe(pipe_fd)`, fork of two child processes, `dup2` descriptor binding, and simultaneous parent/child cleanup of unused pipe ends.

### 8. Background Execution (`&`)
- **Command**:
  ```text
  NexShell> sleep 5 &
  ```
- **Expected Behavior**: Displays `[Background process started: PID <pid>]` and immediately returns the `NexShell> ` prompt for subsequent user interaction without blocking.
- **Verification**: Validates non-blocking child spawning and automatic zombie process cleanup on subsequent loop iterations using `waitpid(-1, NULL, WNOHANG)`.

---

## File Descriptor Handling

In Unix-like operating systems, processes manage I/O streams using integer file descriptors assigned by the kernel:
- **`stdin`  = 0** (`STDIN_FILENO`): Standard input stream (default: keyboard).
- **`stdout` = 1** (`STDOUT_FILENO`): Standard output stream (default: terminal display).
- **`stderr` = 2** (`STDERR_FILENO`): Standard error stream (default: terminal display).

### Stream Redirection via `dup2()`
NexShell uses `dup2(int oldfd, int newfd)` to duplicate open file descriptors onto standard stream slots:
- **Output Redirection (`>`)**: `dup2(output_fd, STDOUT_FILENO)` duplicates the writable file descriptor onto descriptor `1`, routing command output into the destination file.
- **Input Redirection (`<`)**: `dup2(input_fd, STDIN_FILENO)` duplicates the readable file descriptor onto descriptor `0`, allowing the command to read its input directly from the file.
- **Command Pipelines (`|`)**: Duplicates the pipe's write end (`pipe_fd[1]`) onto `STDOUT_FILENO` for the left child and the pipe's read end (`pipe_fd[0]`) onto `STDIN_FILENO` for the right child.

### Descriptor Cleanup and Hygiene
To prevent descriptor exhaustion and resource leaks, file descriptors are closed with `close()` immediately after duplication or when no longer required:
- In redirection, the original `output_fd` or `input_fd` is closed right after `dup2()`, leaving only the standard stream connected.
- In pipelines, unused pipe descriptors are closed in both child processes and the parent process, ensuring clean stream termination and proper End-of-File (EOF) signalling.

---

## Technical Documentation & Specifications Index

NexShell includes comprehensive technical documentation, architecture specifications, test plans, and developer guides:

### Core Architecture & System Specifications
- **[Architecture & Execution Design](docs/ARCHITECTURE.md)**: Detailed breakdown of the REPL loop, input trimming, command parsing, process synchronization, and descriptor flows.
- **[POSIX System Call Reference](docs/SYSTEM_CALLS.md)**: Complete guide to `fork()`, `execvp()`, `waitpid()`, `pipe()`, `dup2()`, `open()`, `close()`, and `chdir()`.
- **[Process Lifecycle Specification](docs/PROCESS_LIFECYCLE.md)**: Unified educational guide detailing process memory models, state transitions (Running, Sleeping, Zombie), and control flows across foreground, pipeline, and background execution.
- **[IPC & File Descriptors](docs/IPC_AND_FILE_DESCRIPTORS.md)**: POSIX file descriptor table mechanics, `dup2` atomic stream redirection, and anonymous pipe buffer management.

### Pipeline Subsystem (`|`)
- **[Pipe Architecture Specification](docs/PIPE_ARCHITECTURE.md)**: Deep dive into POSIX kernel pipe ring buffers, file descriptor table evolution, process tree topology, and EOF propagation.
- **[Pipe Code Walkthrough](docs/PIPE_CODE_WALKTHROUGH.md)**: Line-by-line technical code trace of pipeline parsing, tokenization, descriptor binding, and process synchronization in `main.c`.
- **[Pipe Test Plan & Matrix](docs/PIPE_TEST_PLAN.md)**: Verification matrix covering 14 pipeline testing scenarios, syntax edge cases, and regression verifications.
- **[Pipe Debugging Guide](docs/PIPE_DEBUGGING.md)**: Troubleshooting protocols for pipe hangs, unclosed write descriptors, `dup2` failures, `strace` tracing, and GDB multi-process debugging.
- **[Piping & Background Execution Guide](docs/PIPE_AND_BACKGROUND.md)** & **[Pipe Testing Scenarios](docs/PIPE_TESTING.md)**: Quick reference overview and scenario guide.

### Background Subsystem (`&`)
- **[Background Execution Guide](docs/BACKGROUND_EXECUTION_GUIDE.md)**: Architectural analysis of asynchronous process management, zombie process lifecycle, non-blocking `WNOHANG` reaping, and POSIX job-control comparison.
- **[Background Test Plan & Matrix](docs/BACKGROUND_TEST_PLAN.md)**: Verification matrix covering 11 background execution tests, PID reporting, prompt availability, and zombie reaping.
- **[Background Debugging Guide](docs/BACKGROUND_DEBUGGING.md)**: Troubleshooting protocols for zombie processes (`<defunct>`), process state inspection via `/proc/<pid>/status`, and GDB fork handling.

### Redirection Subsystem (`>`, `<`)
- **[Redirection Architecture & Engineering Guide](docs/REDIRECTION_GUIDE.md)**: Deep dive into `RedirectionInfo`, lexical parsing, child stream substitution, file modes (`O_CREAT`, `O_TRUNC`, `O_RDONLY`), and descriptor isolation.
- **[Redirection Code Walkthrough](docs/REDIRECTION_CODE_WALKTHROUGH.md)**: Line-by-line execution walkthrough of `parse_redirection()` and `execute_child_redirection()`.
- **[Redirection Implementation Guide](docs/REDIRECTION.md)**: Technical breakdown of child process isolation, descriptor redirection, and file mode flags.
- **[Redirection Test Plan & Matrix](docs/REDIRECTION_TEST_PLAN.md)**: Comprehensive 27-scenario test verification matrix.
- **[Redirection Reference Test Suite](tests/redirection/README.md)**: Interactive test command script and edge case reference matrix.
- **[Redirection Testing Guide](docs/REDIRECTION_TESTING.md)**: Practical test scenarios for redirection.
- **[Redirection Debugging & Diagnostic Guide](docs/REDIRECTION_DEBUGGING.md)**: Troubleshooting common system call errors (`ENOENT`, `EACCES`, `EBADF`) and `/proc/<pid>/fd/` inspection.

### Testing & Quality Assurance
- **[Master Test Plan & QA Strategy](docs/TEST_PLAN.md)**: Testing objectives, test environments, boundary conditions, and acceptance criteria.
- **[Official Test Execution Results](docs/TEST_RESULTS.md)**: Live verification logs across standardized functional and regression test cases.
- **[Basic & Built-in Commands Test Suite](tests/basic_commands.md)**: Test specs for `pwd`, `ls`, `cd`, `mkdir`, `exit`, and whitespace sanitization.
- **[I/O Redirection Test Suite](tests/redirection_tests.md)**: Test specs for `>`, `<`, tight syntax, dual redirection, and error handling.
- **[Command Piping Test Suite](tests/pipe_tests.md)**: Test specs for single pipes, multi-arg pipelines, and stream filtering.
- **[Background Execution Test Suite](tests/background_tests.md)**: Test specs for `&` asynchronous execution and zombie cleanup.
- **[Error Handling & Edge Cases Test Suite](tests/error_tests.md)**: Test specs for missing binaries, invalid paths, and repeated operators.
- **[End-to-End Integration Test Suite](tests/integration_tests.md)**: Full session lifecycle integration workflows.

### Developer & Operational Guides
- **[Developer & Contributor Guide](docs/DEVELOPER_GUIDE.md)**: Instructions for adding new built-in commands, extending features, and debugging with GDB/Valgrind.
- **[Troubleshooting & Diagnostic Guide](docs/TROUBLESHOOTING.md)**: Solutions and root-cause analyses for build errors, permission issues, WSL quirks, and GitHub auth.
- **[3–5 Minute Jury Demonstration Script](docs/DEMO_GUIDE.md)**: Timed presentation walkthrough and talking points for project evaluators.
- **[Limitations & Future Technical Scope](docs/LIMITATIONS_AND_FUTURE.md)**: Known architectural constraints and roadmap for multi-pipe chaining, quoting, and signal handling.

---

---

## 9. Limitations
- **No Advanced Shell Quoting**: Does not parse quotes (`"` or `'`) for preserving whitespace inside arguments.
- **No Glob/Wildcard Expansion**: Does not expand filesystem wildcards (`*`, `?`).
- **Simplified Job Control**: Supports asynchronous background execution (`&`), but does not include full job control commands (`jobs`, `fg`, `bg`) or interactive signal trapping (`Ctrl+C`, `Ctrl+Z`).
- **OS Scope**: Built specifically for POSIX-compliant Unix/Linux/WSL systems.

---

## 10. Future Enhancements
- **Multi-stage Command Pipelines**: Support for multi-pipe chaining (`cmd1 | cmd2 | cmd3 | cmd4`).
- **Operator Combining**: Support for combining redirection with pipelines (`cat in.txt | grep error > log.txt`).
- **Full Signal Handling**: Custom signal handlers for `SIGINT` (`Ctrl+C`) and `SIGTSTP` (`Ctrl+Z`) to prevent accidental shell termination.
- **Job Control Management**: Implementing `jobs`, `fg`, and `bg` built-in commands with process group tracking.
- **Readline Integration**: History navigation and tab auto-completion using the GNU `readline` library.

---

## Team Members
- **Susheel**
- **Jaswant**
- **Manoj**
- **Sanjana**
