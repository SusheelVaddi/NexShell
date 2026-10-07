# NexShell Developer & Contributor Guide

This guide is written for students, systems programmers, and developers who wish to understand the inner workings of **NexShell**, extend its capabilities, add new built-in commands, implement advanced shell features, and debug low-level process behaviors using GDB.

---

## 1. Project Organization & Codebase Overview

```
NexShell/
├── main.c                      # Complete C source code & REPL implementation
├── README.md                   # Primary project documentation & architecture index
├── presentation.html           # Interactive HTML5/CSS hackathon slide deck
├── docs/                       # Architectural specs & reference guides
│   ├── ARCHITECTURE.md         # Detailed execution flow and memory model
│   ├── SYSTEM_CALLS.md         # POSIX system call reference
│   ├── TEST_PLAN.md            # Official test plan & acceptance strategy
│   ├── TEST_RESULTS.md         # Live execution verification log
│   ├── TROUBLESHOOTING.md      # Diagnostic & resolution guide
│   ├── DEVELOPER_GUIDE.md      # Developer & extension manual (this document)
│   ├── DEMO_GUIDE.md           # 3-5 minute live jury demo script
│   ├── LIMITATIONS_AND_FUTURE.md # Known constraints and roadmap
│   ├── PIPE_AND_BACKGROUND.md  # Deep dive into pipes and async tasks
│   ├── PIPE_TESTING.md         # Pipe testing notes
│   ├── REDIRECTION.md          # Deep dive into I/O redirection
│   └── REDIRECTION_TESTING.md  # Redirection testing notes
└── tests/                      # Dedicated markdown test suites
    ├── basic_commands.md
    ├── redirection_tests.md
    ├── pipe_tests.md
    ├── background_tests.md
    ├── error_tests.md
    └── integration_tests.md
```

---

## 2. Core Source Code Architecture (`main.c`)

### 2.1 Key Constants & Data Structures
```c
#define MAX_INPUT_SIZE 1024  // Maximum characters per user command line
#define MAX_ARGS 64         // Maximum arguments supported per command

typedef struct {
    char *cmd_part;
    char *input_file;
    char *output_file;
    int has_input_redirect;
    int has_output_redirect;
} RedirectionInfo;
```

### 2.2 Execution Pipeline Steps
1. **Zombie Reaping**: `while (waitpid(-1, NULL, WNOHANG) > 0)` cleans terminated background tasks.
2. **Input Reading**: `fgets(input, sizeof(input), stdin)` reads user input.
3. **Background Check**: `strrchr(input, '&')` extracts background flag.
4. **Whitespace Trimming**: `trim_whitespace(input)` strips leading/trailing blanks.
5. **Built-ins**: Direct execution of `exit` or `cd` via `chdir()`.
6. **Pipeline**: `pipe()`, dual `fork()`, `dup2()` descriptor mapping.
7. **Redirection**: `parse_redirection()` + `execute_child_redirection()`.
8. **External Commands**: Single `fork()` + `execvp()` + `waitpid()`.

---

## 3. How to Extend NexShell

### 3.1 Adding a New Built-in Command (e.g., `help` or `version`)
Because built-in commands run in the parent process, add their logic before the operator and external command dispatchers:

```c
// Example: Adding built-in 'version' command
if (strcmp(trimmed_input, "version") == 0) {
    printf("NexShell Version 1.1 (POSIX C Edition)\n");
    printf("Authors: Susheel, Jaswant, Manoj, Sanjana\n");
    continue;
}
```

```c
// Example: Adding built-in 'help' command
if (strcmp(trimmed_input, "help") == 0) {
    printf("NexShell Built-in Commands:\n");
    printf("  cd [dir]    - Change working directory\n");
    printf("  exit        - Terminate shell session\n");
    printf("  help        - Display this menu\n");
    printf("Supported Operators: >, <, |, &\n");
    continue;
}
```

---

### 3.2 Adding Environment Variable Support (`export` / `setenv`)
To allow setting environment variables in the parent shell:

```c
if (strncmp(trimmed_input, "export", 6) == 0 && (trimmed_input[6] == ' ' || trimmed_input[6] == '\t')) {
    char *assignment = trim_whitespace(trimmed_input + 6);
    char *eq = strchr(assignment, '=');
    if (eq != NULL) {
        *eq = '\0';
        char *name = trim_whitespace(assignment);
        char *val = trim_whitespace(eq + 1);
        setenv(name, val, 1);
    } else {
        printf("Usage: export NAME=VALUE\n");
    }
    continue;
}
```

---

## 4. Debugging with GDB & Valgrind

### 4.1 Compiling for Debugging
Compile with debug symbols (`-g3`) and warnings enabled:
```bash
gcc -g3 -Wall -Wextra main.c -o nexshell_debug
```

### 4.2 Debugging Child Processes in GDB
By default, GDB follows the parent process across `fork()`. To debug the child process instead:
```gdb
gdb ./nexshell_debug
(gdb) set follow-fork-mode child
(gdb) break execute_child_redirection
(gdb) run
```

To debug both parent and child simultaneously:
```gdb
(gdb) set detach-on-fork off
(gdb) info inferiors
```

### 4.3 Memory Leak & Descriptor Leak Detection with Valgrind
```bash
valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes ./nexshell
```

---

## 5. Coding Standards & Best Practices
- **No Global Variables**: Keep state local or pass context structures.
- **Always Check Return Values**: Every call to `fork`, `pipe`, `dup2`, `open`, and `chdir` must check for return values `< 0`.
- **Always Close Unused File Descriptors**: Failing to close pipe ends prevents reader processes from receiving `EOF`.
- **Maintain Clean Commit Hygiene**: Never commit binaries, `.vscode/`, or temporary files.
