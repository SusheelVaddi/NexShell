# NexShell - Technical Implementation Notes

## Architecture Overview
NexShell is a lightweight, POSIX-compliant C shell implementing modular process isolation, arbitrary N-stage pipeline execution, file descriptor redirection, background job tracking, and signal handling.

---

## 1. REPL & Command Loop
The Read-Eval-Print Loop (REPL) operates as follows:
1. **Reap Background Jobs**: Polls `waitpid(-1, NULL, WNOHANG)` to clean up terminated background processes.
2. **Render Dynamic Prompt**: Retrieves current working directory via `getcwd()` and prints `NexShell> ` (or `NexShell:<path>> `).
3. **Read Input**: Reads line via `fgets(input, MAX_LINE, stdin)`. If interrupted by `SIGINT`, clears stream error with `clearerr(stdin)` and re-prompts.
4. **Tokenization & Parsing**: Trims trailing newline, strips background flag `&`, checks for pipe delimiters `|`, and evaluates redirection symbols `<` and `>`.

---

## 2. Process Creation & Execution
- **Builtin Interception**: Builtin commands (`cd`, `pwd`, `mkdir`, `exit`) execute directly within the parent process context.
- **External Commands**: Executed by calling `fork()` to create a child process, followed by `execvp()` in the child. The parent blocks on `waitpid(pid, &status, 0)` unless background execution (`&`) is requested.

---

## 3. Multi-Stage Pipeline Implementation (`N` Stages, `N-1` Pipes)
Multi-stage pipelines support arbitrary command chaining (e.g., `cmd1 | cmd2 | cmd3 | cmd4`).

### Execution Algorithm:
1. Parse line into `N` individual command stage strings split by `|`.
2. Allocate array of `N-1` pipe pairs: `int pipefds[N-1][2]`.
3. Create all `N-1` POSIX pipes via `pipe()`.
4. Iterate `i` from `0` to `N-1` and `fork()` child process for stage `i`:
   - **Input Connection**: If `i > 0`, redirect `pipefds[i-1][0]` to `STDIN_FILENO` via `dup2()`.
   - **Output Connection**: If `i < N-1`, redirect `pipefds[i][1]` to `STDOUT_FILENO` via `dup2()`.
   - **Redirection Parsing**: Handle stage-specific input (`<`) or output (`>`) redirection files.
   - **Descriptor Closure**: Close ALL pipe end descriptors in child after `dup2()` to prevent descriptor leaks.
   - **Execution**: Invoke `execvp()`.
5. **Parent Cleanup**: Parent closes all `N-1` pipe descriptors immediately after spawning all child processes.
6. **Parent Waiting**: Parent loops over all child PIDs and calls `waitpid(child_pids[i], NULL, 0)` for foreground pipelines.

---

## 4. Input & Output Redirection
- **Parsing**: `RedirectionInfo` struct stores `input_file` and `output_file` pointers, extracted from command strings.
- **Application**: Applied inside child process after `fork()` and before `execvp()`.
- **Output Redirection (`>`)**: `open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644)`, followed by `dup2(fd, STDOUT_FILENO)`.
- **Input Redirection (`<`)**: `open(filename, O_RDONLY)`, followed by `dup2(fd, STDIN_FILENO)`.

---

## 5. Signal Handling Architecture
- **Parent Shell**: Installs `signal(SIGINT, SIG_IGN)` at startup. Prevents `Ctrl+C` from killing NexShell. Interrupted `fgets()` calls reset stream flags using `clearerr(stdin)`.
- **Foreground Children**: Explicitly set `signal(SIGINT, SIG_DFL)` after `fork()` to allow standard process termination on `Ctrl+C`.
- **Background Children**: Set `signal(SIGINT, SIG_IGN)` so shell `Ctrl+C` does not affect background jobs.

---

## 6. Known Limitations
1. **Environment Variables**: Variable expansion (e.g., `$PATH`, `$HOME`) is not natively expanded inside raw prompt strings.
2. **Quote Handling**: Literal escaped quotes (e.g., `echo "hello world"`) treat spaces inside quotes as token delimiters.
3. **Job Control Flags**: `ctrl+z` (`SIGTSTP`) and `fg`/`bg` builtins are not implemented.
