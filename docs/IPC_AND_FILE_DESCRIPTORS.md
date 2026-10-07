# NexShell Inter-Process Communication & File Descriptors

This document provides a deep technical analysis of Inter-Process Communication (IPC) and File Descriptor (FD) manipulation in **NexShell**.

---

## 1. Fundamentals of POSIX File Descriptors

In POSIX-compliant operating systems, every process possesses a kernel-managed array called the **File Descriptor Table**. Index numbers in this table are unsigned integers called **File Descriptors (FDs)**.

```text
Process File Descriptor Table
+-----------+-----------------------------------+
| Index (FD)| Kernel Open File Table Pointer    |
+-----------+-----------------------------------+
|     0     | -> Standard Input  (Keyboard/File)|  STDIN_FILENO
|     1     | -> Standard Output (Terminal/Pipe)|  STDOUT_FILENO
|     2     | -> Standard Error  (Terminal)     |  STDERR_FILENO
|     3     | -> Pipe Read End / Opened File    |
|     4     | -> Pipe Write End / Opened File   |
+-----------+-----------------------------------+
```

### Standard Descriptors in C:
- `STDIN_FILENO` (`0`): Default source for input operations (`fgets`, `scanf`, `read`).
- `STDOUT_FILENO` (`1`): Default destination for standard output (`printf`, `puts`, `write`).
- `STDERR_FILENO` (`2`): Default destination for error messages (`perror`, `fprintf(stderr, ...)`).

---

## 2. File Descriptor Inheritance & `fork()` Mechanics

When a process invokes `fork()`, the kernel creates a copy of the parent process's file descriptor table for the child process.

```text
Parent Descriptor Table                      Child Descriptor Table
+----+-----------------------+              +----+-----------------------+
| 0  | -> /dev/pts/1         |              | 0  | -> /dev/pts/1         |
| 1  | -> /dev/pts/1         |  fork()      | 1  | -> /dev/pts/1         |
| 2  | -> /dev/pts/1         | -----------> | 2  | -> /dev/pts/1         |
| 3  | -> Kernel Pipe (Read) |              | 3  | -> Kernel Pipe (Read) |
| 4  | -> Kernel Pipe (Write)|              | 4  | -> Kernel Pipe (Write)|
+----+-----------------------+              +----+-----------------------+
```

### Critical Rules of Inheritance:
1. **Shared Open File Descriptions**: Both parent and child descriptors point to the **same underlying kernel open file descriptions**. This means they share file offsets and status flags.
2. **Independent Table Entries**: Modifying a descriptor index inside the child (e.g. closing FD 3 or calling `dup2`) does **not** alter the file descriptor table of the parent process.

---

## 3. Atomic Stream Redirection with `dup2()`

The `dup2(int oldfd, int newfd)` system call duplicates an open file descriptor (`oldfd`) onto another file descriptor index (`newfd`).

```c
// Example from NexShell Left Child (main.c: line 168):
dup2(pipe_fd[1], STDOUT_FILENO);
```

### Atomic Operation Steps inside Kernel:
1. If `newfd` (e.g., FD 1 / `STDOUT_FILENO`) is currently open, `dup2` closes `newfd` silently.
2. The kernel copies the open file reference from `oldfd` (`pipe_fd[1]`) onto `newfd` (`1`).
3. Now, any code writing to `STDOUT_FILENO` (including external executables run via `execvp`) writes directly into `pipe_fd[1]`.

---

## 4. Anonymous POSIX Pipes vs. Named Pipes

| Feature | Anonymous Pipe (`pipe()`) | Named Pipe (FIFO) |
| :--- | :--- | :--- |
| **Creation Method** | `pipe(int pipefd[2])` | `mkfifo(const char *pathname, mode_t mode)` |
| **Filesystem Presence** | None (Lives entirely in kernel RAM) | Present as a special file in directory tree |
| **Process Relationship** | Requires parent/child inheritance (`fork`) | Any unrelated processes can open by path |
| **Usage in NexShell** | **Used for all pipeline commands (`\|`)** | Not used |

---

## 5. Interaction Between Pipes (`|`) and File Redirection (`>`, `<`)

In NexShell architecture, file redirection (`>`, `<`) and command piping (`|`) use the same underlying system call mechanism (`dup2`).

```text
                  +-----------------------------------+
                  |      Stream Manipulation      |
                  +-----------------------------------+
                                    |
                  +-----------------+-----------------+
                  |                                   |
                  v                                   v
    +---------------------------+       +---------------------------+
    | File Redirection ('>','<')|       |  Command Pipelines ('|')  |
    +---------------------------+       +---------------------------+
    | Uses open() on file path  |       | Uses pipe() kernel buffer |
    | dup2(file_fd, STDOUT)     |       | dup2(pipe_fd[1], STDOUT)  |
    | dup2(file_fd, STDIN)      |       | dup2(pipe_fd[0], STDIN)   |
    +---------------------------+       +---------------------------+
```

### Pipeline Redirection Priority Rules in `main.c`:
In `main.c`, the shell checks for operator types in the following precedence order during parsing:
1. **Built-in Commands** (`exit`, `cd`)
2. **Pipe Operator** (`|`)
3. **Redirection Operators** (`>`, `<`)
4. **General Commands**

When `|` is present on a command line, the shell enters the pipeline execution branch, allocating kernel pipes for IPC between left and right subcommands.
