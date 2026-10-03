# NexShell — A Simplified Unix-like Shell in C

## Problem Statement
Command-line shells are a fundamental interface in Unix-like operating systems, responsible for process management, file descriptor redirection, and inter-process communication. Developing a custom Unix-like shell in C provides practical insight into core operating system concepts, system calls, process lifecycles, and low-level I/O manipulation.

---

## Project Overview
**NexShell** is an educational, simplified Unix-like command-line interpreter written in C for Linux and WSL (Windows Subsystem for Linux) environments. It implements a standard Read-Eval-Print Loop (REPL) interface that reads user commands, tokenizes arguments, and interacts directly with the Linux kernel via native POSIX system calls.

NexShell demonstrates fundamental OS principles including process creation (`fork`), program execution (`execvp`), process synchronization (`wait`/`waitpid`), file descriptor redirection (`dup2`/`open`), and inter-process communication (`pipe`).

---

## Features
- **Prompt Display**: Interactive shell prompt (`NexShell> `).
- **Built-in Commands**:
  - `exit`: Cleanly terminates the shell session.
  - `cd`: Changes current working directory in the parent process (supports default `$HOME`).
  - `pwd`: Displays current working directory path.
  - `mkdir`: Creates new directories.
- **General Command Execution**: Executes standard external Linux utilities (`ls`, `cat`, `grep`, `sleep`, `sort`, etc.) using `fork()`, `execvp()`, and `wait()`.
- **Output Redirection (`>`)**: Redirects standard output to write or truncate a destination file.
- **Input Redirection (`<`)**: Redirects standard input to read data from a file.
- **Command Piping (`|`)**: Connects the output of one process directly to the input of another via POSIX pipes.
- **Background Execution (`&`)**: Asynchronously executes commands without blocking the interactive shell prompt.
- **Non-blocking Zombie Cleanup**: Periodically reaps terminated background child processes using `waitpid(-1, NULL, WNOHANG)` to prevent memory/process table leaks.

---

## How It Works
1. **Input Loop**: The main process prints the prompt `NexShell> ` and reads a line of input using `fgets()`.
2. **Parsing & Cleanup**: Trailing newlines and extra whitespace are removed via custom string trimming.
3. **Built-in Handling**: Commands like `exit` and `cd` are executed directly within the parent process (since `cd` must alter the shell's own working directory via `chdir()`).
4. **Operator Interception**:
   - **`&` (Background)**: Strips `&` from the end of the input, sets a background flag, and forks the child process without calling a blocking `wait()`.
   - **`|` (Pipe)**: Creates a pipe (`pipe_fd[2]`), forks two child processes, uses `dup2()` to connect the left child's stdout to the right child's stdin, and executes both concurrently.
   - **`>` / `<` (Redirection)**: Opens target files with `open()`, uses `dup2()` inside the child process to redirect `STDOUT_FILENO` or `STDIN_FILENO`, and runs `execvp()`.
5. **Foreground Waiting**: For normal commands without `&`, the parent calls `waitpid()` to block until child execution completes.

---

## Technologies Used
- **Language**: C (C99 standard)
- **Environment**: Linux / WSL (Ubuntu)
- **Compiler**: GCC (GNU Compiler Collection)
- **APIs**: POSIX System Calls (`unistd.h`, `sys/types.h`, `sys/wait.h`, `fcntl.h`)

---

## How to Run

### Prerequisites
Ensure you have GCC and GNU Make installed on your Linux or WSL system:
```bash
sudo apt update
sudo apt install build-essential
```

### Compilation
Compile `main.c` using GCC:
```bash
gcc -Wall main.c -o nexshell
```

### Execution
Run the compiled binary:
```bash
./nexshell
```

---

## Example Commands

```text
NexShell> pwd
/home/user/NexShell

NexShell> ls
main.c nexshell

NexShell> mkdir demo

NexShell> cd demo

NexShell> cd ..

NexShell> ls > files.txt

NexShell> cat < files.txt
main.c
files.txt
nexshell

NexShell> ls | grep files
files.txt

NexShell> sleep 10 &
[Background process started: PID 14258]

NexShell> pwd
/home/user/NexShell

NexShell> exit
Exiting NexShell...
```

---

## System Calls Used

- **`fork()`**: Clones the calling shell process to create a new child process with its own execution context.
- **`execvp()`**: Replaces the current child process image with a new executable program specified by command name and argument array.
- **`wait()` / `waitpid()`**: Suspends the parent process until child processes terminate, or non-blockingly inspects child exit statuses with `WNOHANG`.
- **`pipe()`**: Allocates a pair of connected file descriptors (`pipe_fd[0]` for reading, `pipe_fd[1]` for writing) for inter-process communication.
- **`dup2()`**: Duplicates an open file descriptor onto `STDIN_FILENO` (0) or `STDOUT_FILENO` (1) for input/output redirection.
- **`chdir()`**: Changes the current working directory of the shell process.
- **`open()` / `close()`**: Opens or creates files with specified flags (`O_WRONLY`, `O_CREAT`, `O_TRUNC`, `O_RDONLY`) and closes unused file descriptors.

---

## Team Members
- **NexShell Developers** (College Hackathon Project)

---

## Limitations
- **Single Pipe Only**: Supports single-pipe constructs (`cmd1 | cmd2`), but does not currently support multi-pipe chains (`cmd1 | cmd2 | cmd3`).
- **No Operator Combinations**: Does not currently combine piping with redirection on a single line (e.g. `ls | grep test > out.txt`).
- **Simplified Job Control**: Supports asynchronous background execution (`&`), but does not include full job control commands (`jobs`, `fg`, `bg`) or terminal signal handlers (`Ctrl+Z`).
- **OS Scope**: Designed specifically for POSIX-compliant Unix/Linux/WSL operating systems.

---

## Future Scope
- Support for multi-stage command pipelines (`a | b | c | d`).
- Support for complex operator combinations (`cat input.txt | grep error > log.txt`).
- Full signal handling (`SIGINT`, `SIGTSTP`, `Ctrl+C`, `Ctrl+Z`).
- Job control management (`jobs`, `fg`, `bg`).
- Integration of `readline` library for command history and arrow-key cursor navigation.
