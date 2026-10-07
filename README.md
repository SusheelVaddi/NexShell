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
- **Output Redirection (`>`)**: Redirects standard output (`stdout`) to write or truncate a destination file.
- **Input Redirection (`<`)**: Redirects standard input (`stdin`) to read data directly from a file.
- **Dual Redirection (`<` and `>`)**: Supports simultaneous input and output redirection on a single command line (e.g., `cmd < input.txt > output.txt` or `cmd > output.txt < input.txt`).
- **Command Piping (`|`)**: Inter-process communication connecting the standard output of a left child process directly to the standard input of a right child process using POSIX `pipe()` and `dup2()`.
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
|    - Redirection ('>', '<')  -> Open files, fork child, dup2 FDs      |
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
4. **Redirection Flow**: When `>` or `<` is detected, target filenames are extracted and trimmed. The child process opens files with `open()` (`O_WRONLY | O_CREAT | O_TRUNC` for output; `O_RDONLY` for input) and redirects standard descriptors via `dup2()`.

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

## 8. Limitations
- **Single Pipe Only**: Supports single-pipe constructs (`cmd1 | cmd2`), but does not support multi-pipe chains (`cmd1 | cmd2 | cmd3`).
- **No Operator Combinations**: Does not support combining redirection with piping on a single command line (e.g., `ls | grep test > out.txt`).
- **No Advanced Shell Quoting**: Does not parse quotes (`"` or `'`) for preserving whitespace inside arguments.
- **No Glob/Wildcard Expansion**: Does not expand filesystem wildcards (`*`, `?`).
- **Simplified Job Control**: Supports asynchronous background execution (`&`), but does not include full job control commands (`jobs`, `fg`, `bg`) or interactive signal trapping (`Ctrl+C`, `Ctrl+Z`).
- **OS Scope**: Built specifically for POSIX-compliant Unix/Linux/WSL systems.

---

## 9. Future Enhancements
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
