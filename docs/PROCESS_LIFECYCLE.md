# NexShell Process Lifecycle Specification

This educational document provides a complete unified specification of the process lifecycle in **NexShell**. It contrasts the execution control flows, memory models, descriptor tables, and state transitions across **Foreground Commands**, **Command Pipelines**, and **Background Execution**.

---

## 1. Unified Process Lifecycle Overview

Operating systems isolate program execution into **processes**. A process consists of:
- Virtual address space (Text, Data, Heap, Stack).
- CPU register context (Instruction Pointer, Stack Pointer).
- Kernel file descriptor table (mapping integer descriptors 0, 1, 2... to open files/devices).
- Process state (Running, Sleeping, Zombie, Terminated).

NexShell serves as the parent process (`PPID`), managing child processes (`PID`) via POSIX system calls (`fork`, `execvp`, `waitpid`, `pipe`, `dup2`).

---

## 2. Execution Control Flows

### Mode 1: Standard Foreground Command (`pwd`, `ls -l`)

In foreground execution, the parent shell delegates execution to a single child process and pauses until the child terminates.

```text
       NexShell Parent (REPL)                            Child Process
      +-----------------------+                        +---------------+
      | 1. Read input "pwd"   |                        |               |
      | 2. Call fork()        | ---------------------> | 1. Created    |
      | 3. Call waitpid(pid)  | (Parent Blocks)        | 2. execvp()   |
      |    [PAUSED]           |                        | 3. Runs pwd   |
      |                       |                        | 4. exit(0)    |
      |                       | <--------------------- | 5. Terminated |
      | 4. Unblocked by Kernel| (State Collected)      +---------------+
      | 5. Re-display Prompt  |
      +-----------------------+
```

#### Lifecycle Step Sequence:
1. **Parent**: Reads input line, tokenizes arguments (`args = ["pwd", NULL]`).
2. **Fork**: `pid = fork()` clones the parent shell process.
3. **Child Branch (`pid == 0`)**: Calls `execvp("pwd", args)`. The kernel overwrites the child memory image with `/bin/pwd` executable binary code.
4. **Parent Branch (`pid > 0`)**: Calls `waitpid(pid, NULL, 0)`. The OS suspends the parent process thread.
5. **Termination & Reaping**: When `pwd` finishes, it calls `exit(0)`. The kernel wakes up the parent, delivers the exit status, destroys the child process entry, and unblocks `waitpid()`.

---

### Mode 2: Pipeline Execution (`ls | sort`)

In pipeline execution, the parent shell creates a kernel pipe and spawns two concurrent child processes whose standard streams are tied to opposite ends of the pipe.

```text
                 +-------------------------------------------------+
                 |                NexShell Parent                  |
                 +-------------------------------------------------+
                    /                 |                 \
     1. pipe(fd)   /   2. fork()      |   3. fork()      \   4. waitpid(pid1)
                  /    (Left Child)   |   (Right Child)   \     waitpid(pid2)
                 v                    v                    v
       +------------------+  Pipe Data Stream  +------------------+
       | Left Child (ls)  | =================> |Right Child (sort)|
       | STDOUT -> fd[1]  |                    | STDIN <- fd[0]   |
       +------------------+                    +------------------+
```

#### Lifecycle Step Sequence:
1. **Pipe Allocation**: `pipe(pipe_fd)` allocates read descriptor `pipe_fd[0]` and write descriptor `pipe_fd[1]`.
2. **Left Child Spawning (`pid1`)**:
   - `pid1 = fork()` creates Left Child.
   - Left Child calls `dup2(pipe_fd[1], STDOUT_FILENO)` to route standard output into the pipe.
   - Closes `pipe_fd[0]` and `pipe_fd[1]`.
   - Calls `execvp("ls", left_args)`.
3. **Right Child Spawning (`pid2`)**:
   - `pid2 = fork()` creates Right Child.
   - Right Child calls `dup2(pipe_fd[0], STDIN_FILENO)` to route standard input from the pipe.
   - Closes `pipe_fd[0]` and `pipe_fd[1]`.
   - Calls `execvp("sort", right_args)`.
4. **Parent Descriptor Cleanup & Wait**:
   - Parent closes `pipe_fd[0]` and `pipe_fd[1]`.
   - Calls `waitpid(pid1, NULL, 0)` followed by `waitpid(pid2, NULL, 0)`.
   - When both `ls` and `sort` finish, parent unblocks and redisplays `NexShell> `.

---

### Mode 3: Background Execution (`sleep 10 &`)

In background execution, the parent shell spawns a child process but **does not block**. Control returns immediately to the prompt.

```text
       NexShell Parent (REPL)                            Child Process
      +-----------------------+                        +---------------+
      | 1. Read "sleep 10 &"  |                        |               |
      | 2. Parse '&' (bg=1)   |                        |               |
      | 3. Call fork()        | ---------------------> | 1. Created    |
      | 4. Print PID          |                        | 2. execvp()   |
      | 5. Re-display Prompt  |                        | 3. Sleep 10s  |
      | 6. Accept New Inputs  |                        |               |
      |                       |                        | 4. exit(0)    |
      |                       |                        v               |
      | 7. Loop Start:        |                     +------------------+
      |    waitpid(...,       | <------------------ | Zombie Process   |
      |            WNOHANG)   | (Reaped silently)   +------------------+
      +-----------------------+
```

#### Lifecycle Step Sequence:
1. **Parent Parsing**: Detects trailing `&`, sets `is_background = 1`, and strips `&`.
2. **Child Spawning**: `pid = fork()` creates child process.
3. **Child Execution**: Calls `execvp("sleep", ["sleep", "10", NULL])`.
4. **Immediate Parent Return**: Parent prints PID and bypasses blocking `waitpid()`, returning immediately to REPL prompt.
5. **Asynchronous Reaping**: When `sleep` terminates 10 seconds later, it becomes a Zombie. On the user's next command input, `while (waitpid(-1, NULL, WNOHANG) > 0)` in `main.c` reaps the zombie process non-blockingly.

---

## 3. Comprehensive Lifecycle Comparison Matrix

| Property | Foreground Command | Pipeline (`cmd1 \| cmd2`) | Background Command (`cmd &`) |
| :--- | :--- | :--- | :--- |
| **Child Processes Forked** | 1 (`pid`) | 2 (`pid1`, `pid2`) | 1 (`pid`) |
| **Parent Wait Mode** | Blocking `waitpid(pid, NULL, 0)` | Blocking `waitpid(pid1)` + `waitpid(pid2)` | Non-blocking `waitpid(-1, NULL, WNOHANG)` |
| **Prompt Availability** | Blocked until child finishes | Blocked until both children finish | **Immediate** prompt availability |
| **Standard Stream Binding**| Unchanged (Terminal stdout/stdin) | Left stdout -> Pipe write<br>Right stdin <- Pipe read | Unchanged (Shares terminal stdout/stdin) |
| **Zombie Risk** | Zero (Parent waits synchronously) | Zero (Parent waits synchronously) | Mitigated via `WNOHANG` reaping loop |
| **PID Reporting** | None | Reported if background pipeline | `[Background process started: PID %d]` |

---

## 4. Kernel Process State Transitions

In all three execution modes, every child process transitions through kernel process states (`task_struct->state`):

```text
               +-------------------+
               |  TASK_RUNNING     | <--- Process created via fork()
               +-------------------+
                         |
                         | System call / I/O wait
                         v
               +-------------------+
               | TASK_INTERRUPTIBLE| (e.g. sleep 10)
               +-------------------+
                         |
                         | Timer expire / Exit call
                         v
               +-------------------+
               |  EXIT_ZOMBIE      | <--- Exit status saved in kernel
               +-------------------+
                         |
                         | Reaped via waitpid()
                         v
               +-------------------+
               | PROCESS DESTROYED | <--- Task struct removed from RAM
               +-------------------+
```
