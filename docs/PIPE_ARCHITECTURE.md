# NexShell Pipe Architecture Specification

This document provides a comprehensive architectural specification of the single-pipe (`|`) execution subsystem in **NexShell**. It covers low-level kernel abstractions, process topology, file descriptor manipulation, IPC byte-stream lifecycle, and error recovery mechanics.

---

## 1. POSIX Pipe Abstraction

A POSIX pipe is a unidirectional inter-process communication (IPC) channel managed directly by the operating system kernel. In Unix-like kernels (Linux, BSD, macOS), a pipe is represented internally as a circular ring buffer (typically 64 KB capacity on Linux) allocated in kernel memory.

```
                  +-----------------------------------+
                  |        Kernel Pipe Buffer         |
                  |  [ Byte Stream Ring Buffer (64KB)] |
                  +-----------------------------------+
                   ^                                 |
                   | Write                           | Read
                   |                                 v
   +-------------------------------+   +-------------------------------+
   | Producer Process (Left Child) |   | Consumer Process (Right Child)|
   | Descriptor 1 (STDOUT)         |   | Descriptor 0 (STDIN)          |
   +-------------------------------+   +-------------------------------+
```

### Key Properties:
- **Unidirectional Flow**: Bytes written to the write end of the pipe are read from the read end in strict First-In, First-Out (FIFO) order.
- **Atomic Operations**: Writes up to `PIPE_BUF` (4096 bytes on Linux) are guaranteed to be atomic across concurrent processes.
- **Blocking Read Semantics**: A read request on an empty pipe buffer blocks the calling process until data is written to the pipe or all write descriptors are closed.
- **Blocking Write Semantics**: A write request to a full pipe buffer blocks the calling process until space becomes available.
- **End-of-File (EOF) Signal**: A read operation returns `0` bytes (EOF) only when the pipe buffer is empty **and** all write file descriptors referencing the pipe across all processes are closed.

---

## 2. NexShell Pipeline Topology

NexShell implements single-pipe execution (`cmd1 | cmd2`) by creating a parent shell process and spawning two concurrent child processes connected via a kernel pipe.

```
                                +-------------------+
                                |   NexShell Parent |
                                |   (REPL Loop)     |
                                +-------------------+
                                  /               \
                    pid1 = fork() /                 \ pid2 = fork()
                                 /                   \
                                v                     v
                    +--------------------+   +--------------------+
                    | Left Child Process |   | Right Child Process|
                    | (Executing cmd1)   |   | (Executing cmd2)   |
                    +--------------------+   +--------------------+
                    | STDOUT -> Pipe Write|   | STDIN <- Pipe Read |
                    +--------------------+   +--------------------+
```

### System Call Component Roles

| System Call | Prototype | Role in NexShell Pipeline |
| :--- | :--- | :--- |
| `pipe()` | `int pipe(int pipefd[2])` | Allocates kernel pipe buffer and returns two file descriptors: `pipefd[0]` (read) and `pipefd[1]` (write). |
| `fork()` | `pid_t fork(void)` | Clones the parent process image. Called twice to create `pid1` (left child) and `pid2` (right child). |
| `dup2()` | `int dup2(int oldfd, int newfd)` | Replaces `STDOUT_FILENO` in Left Child with `pipe_fd[1]`, and `STDIN_FILENO` in Right Child with `pipe_fd[0]`. |
| `close()` | `int close(int fd)` | Closes unused file descriptors in parent and both children to prevent descriptor leaks and enable EOF signaling. |
| `execvp()`| `int execvp(const char *file, char *const argv[])` | Replaces child process memory image with executable binary specified in command tokens. |
| `waitpid()`| `pid_t waitpid(pid_t pid, int *status, int options)` | Pauses parent shell execution until `pid1` and `pid2` complete (foreground pipeline execution). |

---

## 3. File Descriptor Table Evolution

To understand how streams are redirected without affecting the parent process, consider the step-by-step transformation of process file descriptor (FD) tables.

### Phase 1: Parent Shell Initial State
Before executing `cmd1 | cmd2`, the parent process has standard descriptors mapped to the terminal device (`/dev/pts/X`):

| Descriptor (FD) | Stream | Target File / Device |
| :--- | :--- | :--- |
| `0` | `STDIN_FILENO` | Terminal Input (`/dev/pts/X`) |
| `1` | `STDOUT_FILENO` | Terminal Output (`/dev/pts/X`) |
| `2` | `STDERR_FILENO` | Terminal Error (`/dev/pts/X`) |

---

### Phase 2: Pipe Allocation (`pipe(pipe_fd)`)
`pipe()` allocates two new descriptors in the parent process table:

| Descriptor (FD) | Stream | Target File / Device |
| :--- | :--- | :--- |
| `0` | `STDIN_FILENO` | Terminal Input (`/dev/pts/X`) |
| `1` | `STDOUT_FILENO` | Terminal Output (`/dev/pts/X`) |
| `2` | `STDERR_FILENO` | Terminal Error (`/dev/pts/X`) |
| `3` | `pipe_fd[0]` | **Kernel Pipe (Read End)** |
| `4` | `pipe_fd[1]` | **Kernel Pipe (Write End)** |

---

### Phase 3: Left Child Setup (`pid1 == 0`)
1. `pid1 = fork()` copies the parent's descriptor table (FD 0-4) into Left Child memory space.
2. `dup2(pipe_fd[1], STDOUT_FILENO)` (i.e. `dup2(4, 1)`) overwrites descriptor `1` with descriptor `4`.
3. Left child invokes `close(pipe_fd[0])` (`close(3)`) and `close(pipe_fd[1])` (`close(4)`).

**Left Child Descriptor Table before `execvp()`**:
| Descriptor (FD) | Stream | Target File / Device |
| :--- | :--- | :--- |
| `0` | `STDIN_FILENO` | Terminal Input (`/dev/pts/X`) |
| `1` | `STDOUT_FILENO` | **Kernel Pipe (Write End)** (via `dup2`) |
| `2` | `STDERR_FILENO` | Terminal Error (`/dev/pts/X`) |
| `3` | `pipe_fd[0]` | *CLOSED* |
| `4` | `pipe_fd[1]` | *CLOSED* |

---

### Phase 4: Right Child Setup (`pid2 == 0`)
1. `pid2 = fork()` copies the parent's descriptor table into Right Child memory space.
2. `dup2(pipe_fd[0], STDIN_FILENO)` (i.e. `dup2(3, 0)`) overwrites descriptor `0` with descriptor `3`.
3. Right child invokes `close(pipe_fd[0])` (`close(3)`) and `close(pipe_fd[1])` (`close(4)`).

**Right Child Descriptor Table before `execvp()`**:
| Descriptor (FD) | Stream | Target File / Device |
| :--- | :--- | :--- |
| `0` | `STDIN_FILENO` | **Kernel Pipe (Read End)** (via `dup2`) |
| `1` | `STDOUT_FILENO` | Terminal Output (`/dev/pts/X`) |
| `2` | `STDERR_FILENO` | Terminal Error (`/dev/pts/X`) |
| `3` | `pipe_fd[0]` | *CLOSED* |
| `4` | `pipe_fd[1]` | *CLOSED* |

---

### Phase 5: Parent Cleanup (`parent process`)
Immediately after forking `pid2`, the parent process closes its copy of `pipe_fd[0]` and `pipe_fd[1]`:
```c
close(pipe_fd[0]);
close(pipe_fd[1]);
```

**Parent Descriptor Table after Cleanup**:
| Descriptor (FD) | Stream | Target File / Device |
| :--- | :--- | :--- |
| `0` | `STDIN_FILENO` | Terminal Input (`/dev/pts/X`) |
| `1` | `STDOUT_FILENO` | Terminal Output (`/dev/pts/X`) |
| `2` | `STDERR_FILENO` | Terminal Error (`/dev/pts/X`) |
| `3` | `pipe_fd[0]` | *CLOSED* |
| `4` | `pipe_fd[1]` | *CLOSED* |

The parent's file descriptor table returns to its baseline state, ensuring subsequent shell commands are not corrupted by open pipe descriptors.

---

## 4. End-of-File (EOF) & Descriptor Closing Rules

The kernel maintains reference counts for every open file descriptor targeting an underlying kernel object.

```
                                  Open Write References Count
                                 +---------------------------+
                                 | Left Child FD 1  (+1)      |
                                 | Parent FD 4      (+1)      |
                                 | Right Child FD 4 (+1)      |
                                 +---------------------------+
                                                | Total = 3
```

### Why Closing Pipe Descriptors in Parent and Children is Essential:
1. **Right Child Closing `pipe_fd[1]`**: If the right child (`sort`) leaves `pipe_fd[1]` open, its own process retains a write handle to the pipe. When the left child (`ls`) finishes and exits, the write reference count does not drop to 0. `sort` will block forever on `read()`, waiting for more data.
2. **Parent Closing `pipe_fd[1]`**: If the parent shell fails to close `pipe_fd[1]`, the parent process retains a write handle. Even after `ls` terminates, the kernel sees 1 remaining write handle (held by the parent), so `sort` never receives EOF and deadlocks.
3. **Closing Read Handles (`pipe_fd[0]`)**: Closing unused read handles in the left process and parent prevents file descriptor leaks and frees kernel table entries.

---

## 5. Execution Flow & Process Lifecycle Diagram

```
User Input: "ls | sort"
         |
         v
+------------------+
| Parse & Tokenize |
+------------------+
         |
         v
+------------------+
| Call pipe()      | ---> pipe_fd[0] (read), pipe_fd[1] (write) created
+------------------+
         |
         +----------------------------------+
         |                                  |
         v                                  v
+------------------+              +------------------+
| Fork Left Child  |              | Fork Right Child |
| (pid1)           |              | (pid2)           |
+------------------+              +------------------+
         |                                  |
         v                                  v
+------------------+              +------------------+
| dup2(fd[1], 1)   |              | dup2(fd[0], 0)   |
| close(fd[0],[1]) |              | close(fd[0],[1]) |
| execvp("ls")     |              | execvp("sort")   |
+------------------+              +------------------+
         |                                  |
         +----------------+-----------------+
                          |
                          v
                 +------------------+
                 | Parent Process   |
                 | close(fd[0],[1]) |
                 | waitpid(pid1)    |
                 | waitpid(pid2)    |
                 +------------------+
                          |
                          v
                 +------------------+
                 | Re-display Prompt|
                 | "NexShell> "     |
                 +------------------+
```

---

## 6. Error Handling Architecture

NexShell incorporates structured error paths for pipeline execution:

1. **`pipe()` Failure**:
   - If `pipe(pipe_fd) < 0`, NexShell prints an error via `perror("pipe failed")` and skips process creation, returning control to the prompt.
2. **Left Fork Failure (`pid1 < 0`)**:
   - Both ends of the pipe (`pipe_fd[0]` and `pipe_fd[1]`) are closed immediately to prevent descriptor leaks before continuing the REPL loop.
3. **Right Fork Failure (`pid2 < 0`)**:
   - Both pipe file descriptors are closed.
   - The parent calls `waitpid(pid1, NULL, 0)` to ensure the already-spawned left child is properly reaped, preventing a zombie process.
4. **Child Stream Redirection Failure (`dup2 < 0`)**:
   - If `dup2()` fails inside a child process, the child prints an error message, closes remaining pipe descriptors, and calls `exit(1)` to terminate cleanly.
5. **Executable Lookup Failure (`execvp < 0`)**:
   - If `execvp()` fails (e.g. command binary not found), it prints `perror("execvp failed")` and immediately calls `exit(1)`. The child exits, closing its descriptors and signaling EOF to the downstream process.

---

## 7. Limitations & Multi-Stage Pipeline Analysis

### Current Single-Pipe Constraint
NexShell currently handles single-pipe expressions containing exactly one `|` operator (`cmd1 | cmd2`).

### Architectural Requirements for Multi-Stage Pipelines (`cmd1 | cmd2 | ... | cmdN`)
To support arbitrary $N$-stage pipelines (e.g., `cat access.log | grep 404 | awk '{print $7}' | sort | uniq -c`):
1. **Pipe Array Allocation**: An array of $N-1$ pipes (`int pipe_fds[N-1][2]`) must be created before forking.
2. **Iterative Process Forking**: A loop from $i = 0$ to $N-1$ forks $N$ child processes:
   - Child $0$: Output redirected to `pipe_fds[0][1]`.
   - Child $i$ ($0 < i < N-1$): Input redirected from `pipe_fds[i-1][0]`, output redirected to `pipe_fds[i][1]`.
   - Child $N-1$: Input redirected from `pipe_fds[N-2][0]`.
3. **Descriptor Cleanup Loop**: All $2 \times (N-1)$ pipe file descriptors must be closed in the parent process and inside every child process.
4. **PID Array Waiting**: The parent process must store child PIDs in an array `pid_t pids[N]` and wait for all $N$ processes using a loop over `waitpid()`.
