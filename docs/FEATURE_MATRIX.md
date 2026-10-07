# NexShell - Feature Implementation Matrix

| Requirement | Implementation Architecture | POSIX / C System Calls Used | Verification Test Command | Status |
| :--- | :--- | :--- | :--- | :---: |
| **Interactive REPL** | Command loop reading user input with dynamic prompt displaying active directory | `fgets()`, `getcwd()`, `printf()`, `fflush()`, `clearerr()` | `./nexshell` | **PASSED** |
| **Builtin: `pwd`** | Intercepted in parent shell before forking; outputs current working directory | `getcwd()`, `printf()` | `pwd` | **PASSED** |
| **Builtin: `cd`** | Changes parent shell directory context; supports relative and absolute paths | `chdir()`, `perror()` | `cd ..`, `cd /tmp` | **PASSED** |
| **Builtin: `mkdir`** | Creates new directory with standard permissions (`0755`) | `mkdir()`, `perror()` | `mkdir test_dir` | **PASSED** |
| **Builtin: `exit`** | Gracefully terminates shell execution | `exit()` | `exit` | **PASSED** |
| **External Commands** | Forks child process, parses arguments, and executes target binary via `PATH` | `fork()`, `execvp()`, `waitpid()`, `perror()` | `ls -la` | **PASSED** |
| **Output Redirection (`>`)** | Opens/creates file for writing, redirects `STDOUT_FILENO` in child process | `open()`, `dup2()`, `close()`, `execvp()` | `echo hello > out.txt` | **PASSED** |
| **Input Redirection (`<`)** | Opens file for reading, redirects `STDIN_FILENO` in child process | `open()`, `dup2()`, `close()`, `execvp()` | `cat < out.txt` | **PASSED** |
| **Single Pipe (`\|`)** | Creates 1 pipe channel, forks 2 children, connects stdout of stage 0 to stdin of stage 1 | `pipe()`, `fork()`, `dup2()`, `close()`, `waitpid()` | `ls \| sort` | **PASSED** |
| **Multi-Stage Pipeline (`\|`)** | Creates `N-1` pipes for `N` stages, forks `N` children, wires intermediate stdin/stdout | `pipe()`, `fork()`, `dup2()`, `close()`, `waitpid()` | `cat main.c \| grep fork \| sort \| head` | **PASSED** |
| **Background Execution (`&`)** | Parent skips blocking wait, registers PID; background reaper polls zombie exits | `fork()`, `execvp()`, `waitpid(..., WNOHANG)` | `sleep 3 &` | **PASSED** |
| **Signal Handling (`SIGINT`)** | Parent ignores `SIGINT`, foreground children reset to `SIG_DFL`, background children ignore `SIG_IGN` | `signal(SIGINT, SIG_IGN)`, `signal(SIGINT, SIG_DFL)`, `clearerr()` | `sleep 30` + `Ctrl+C` | **PASSED** |
| **Syntax Validation** | Pre-execution parsing checks missing redirection paths and invalid pipe positions | Internal string parsing | `\| ls`, `ls \|`, `cat <` | **PASSED** |
