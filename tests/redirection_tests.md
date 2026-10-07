# NexShell Test Suite: Input & Output Redirection

This document specifies test scenarios and validation logs for NexShell's I/O redirection subsystem, including standard output redirection (`>`), standard input redirection (`<`), tight syntax, dual redirection, and error handling.

---

## Test Cases

### Test TC-REDIR-01: Standard Output Redirection (`>`)
- **Objective**: Verify standard output is redirected into a destination file, creating the file if missing and truncating if existing.
- **Command**:
  ```text
  NexShell> ls > test_out.txt
  ```
- **Expected Behavior**: No terminal output; `test_out.txt` is created with mode `0644` containing the output of `ls`.
- **Actual Behavior**: `open("test_out.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644)` succeeds; `dup2(fd, STDOUT_FILENO)` routes stdout to file.
- **Feature Being Verified**: Output redirection (`>`), file creation, descriptor duplication.
- **Status**: **PASS**

---

### Test TC-REDIR-02: Overwriting Existing File via Output Redirection
- **Objective**: Verify `O_TRUNC` flag behavior ensuring previous file contents are overwritten.
- **Command**:
  ```text
  NexShell> echo "Initial Line" > test_out.txt
  NexShell> echo "Replacement Line" > test_out.txt
  NexShell> cat < test_out.txt
  ```
- **Expected Behavior**: `test_out.txt` contains only "Replacement Line".
- **Actual Behavior**: File is truncated upon open; contains only the single new line.
- **Feature Being Verified**: `O_TRUNC` file flag behavior.
- **Status**: **PASS**

---

### Test TC-REDIR-03: Standard Input Redirection (`<`)
- **Objective**: Verify standard input is redirected from an existing file into a command's `STDIN_FILENO`.
- **Command**:
  ```text
  NexShell> cat < test_out.txt
  ```
- **Expected Behavior**: `cat` reads from `test_out.txt` and writes contents to the terminal.
- **Actual Behavior**: `open("test_out.txt", O_RDONLY)` succeeds; `dup2(fd, STDIN_FILENO)` binds descriptor 0; output printed.
- **Feature Being Verified**: Input redirection (`<`), `STDIN_FILENO` redirection.
- **Status**: **PASS**

---

### Test TC-REDIR-04: Tight Syntax Output Redirection (`ls>test_out.txt`)
- **Objective**: Verify parser extracts command and filename correctly when no whitespace surrounds `>`.
- **Command**:
  ```text
  NexShell> ls>test_out.txt
  ```
- **Expected Behavior**: Parses command as `ls` and output file as `test_out.txt`.
- **Actual Behavior**: Correctly splits string at `>` pointer and trims whitespace; writes to `test_out.txt`.
- **Feature Being Verified**: Lexical parsing of tight redirection syntax.
- **Status**: **PASS**

---

### Test TC-REDIR-05: Tight Syntax Input Redirection (`cat<test_out.txt`)
- **Objective**: Verify parser extracts command and filename correctly when no whitespace surrounds `<`.
- **Command**:
  ```text
  NexShell> cat<test_out.txt
  ```
- **Expected Behavior**: Parses command as `cat` and input file as `test_out.txt`.
- **Actual Behavior**: Correctly redirects `STDIN_FILENO` and outputs file content.
- **Feature Being Verified**: Lexical parsing of tight input redirection syntax.
- **Status**: **PASS**

---

### Test TC-REDIR-06: Dual Redirection (`cmd < input > output`)
- **Objective**: Verify simultaneous redirection of `stdin` and `stdout` on a single command line.
- **Command**:
  ```text
  NexShell> cat < test_out.txt > test_copy.txt
  ```
- **Expected Behavior**: Reads from `test_out.txt` and writes to `test_copy.txt` without terminal output.
- **Actual Behavior**: Child process binds both `STDIN_FILENO` and `STDOUT_FILENO`; `test_copy.txt` created with identical content.
- **Feature Being Verified**: Combined redirection (`<` followed by `>`).
- **Status**: **PASS**

---

### Test TC-REDIR-07: Dual Redirection Inverted Order (`cmd > output < input`)
- **Objective**: Verify simultaneous redirection when `>` precedes `<`.
- **Command**:
  ```text
  NexShell> cat > test_copy2.txt < test_out.txt
  ```
- **Expected Behavior**: Accurately extracts both filenames and executes command with both descriptors redirected.
- **Actual Behavior**: Parsed and executed correctly; `test_copy2.txt` created with expected content.
- **Feature Being Verified**: Combined redirection (`>` followed by `<`).
- **Status**: **PASS**

---

### Test TC-REDIR-08: Non-Existent Input File
- **Objective**: Verify graceful failure when source file for input redirection does not exist.
- **Command**:
  ```text
  NexShell> cat < nonexistent_file_xyz.txt
  ```
- **Expected Behavior**: Displays `open failed: No such file or directory` and does not crash the shell.
- **Actual Behavior**: `open()` returns `-1`, `perror("open failed")` reports error, child calls `exit(1)`, parent re-prompts.
- **Feature Being Verified**: Redirection error handling and shell crash prevention.
- **Status**: **PASS**

---

### Test TC-REDIR-09: Missing Output Filename
- **Objective**: Verify syntax validation when `>` has no destination filename.
- **Command**:
  ```text
  NexShell> ls >
  ```
- **Expected Behavior**: Displays `Error: Missing output filename.` and skips execution.
- **Actual Behavior**: `parse_redirection()` returns error code; prompt returns immediately.
- **Feature Being Verified**: Redirection syntax validation.
- **Status**: **PASS**

---

### Test TC-REDIR-10: Missing Input Filename
- **Objective**: Verify syntax validation when `<` has no source filename.
- **Command**:
  ```text
  NexShell> cat <
  ```
- **Expected Behavior**: Displays `Error: Missing input filename.` and skips execution.
- **Actual Behavior**: `parse_redirection()` returns error code; prompt returns immediately.
- **Feature Being Verified**: Redirection syntax validation.
- **Status**: **PASS**

---

### Test TC-REDIR-11: Missing Command Before Redirection
- **Objective**: Verify error when redirection operator appears without a preceding command.
- **Command**:
  ```text
  NexShell> > test_out.txt
  NexShell> < test_out.txt
  ```
- **Expected Behavior**: Displays `Error: Missing command before '>'.` / `Error: Missing command before '<'.`.
- **Actual Behavior**: Emits informative error message; no child process spawned.
- **Feature Being Verified**: Command part validation.
- **Status**: **PASS**

---

### Test TC-REDIR-12: Repeated / Multiple Redirection Operators
- **Objective**: Verify rejection of unsupported repeated operators (e.g., `>>` or `<<`).
- **Command**:
  ```text
  NexShell> ls >> test_out.txt
  NexShell> cat << test_out.txt
  ```
- **Expected Behavior**: Displays `Error: Multiple or repeated '>' redirection operators.` / `Error: Multiple or repeated '<' redirection operators.`.
- **Actual Behavior**: Operator count checks (`count_out > 1` / `count_in > 1`) reject malformed input cleanly.
- **Feature Being Verified**: Redirection operator uniqueness validation.
- **Status**: **PASS**

---

## Summary Matrix

| Test ID | Command | Target Mechanism | Result |
| :--- | :--- | :--- | :--- |
| **TC-REDIR-01** | `ls > test_out.txt` | Output redirection (`dup2` + `O_TRUNC`) | **PASS** |
| **TC-REDIR-02** | `echo "New" > test_out.txt` | `O_TRUNC` overwrite verification | **PASS** |
| **TC-REDIR-03** | `cat < test_out.txt` | Input redirection (`dup2` + `O_RDONLY`) | **PASS** |
| **TC-REDIR-04** | `ls>test_out.txt` | Tight syntax parsing | **PASS** |
| **TC-REDIR-05** | `cat<test_out.txt` | Tight syntax parsing | **PASS** |
| **TC-REDIR-06** | `cat < in > out` | Dual redirection (`<` then `>`) | **PASS** |
| **TC-REDIR-07** | `cat > out < in` | Dual redirection (`>` then `<`) | **PASS** |
| **TC-REDIR-08** | `cat < nonexistent.txt` | `open()` failure & error reporting | **PASS** |
| **TC-REDIR-09** | `ls >` | Missing filename validation | **PASS** |
| **TC-REDIR-10** | `cat <` | Missing filename validation | **PASS** |
| **TC-REDIR-11** | `> file.txt` | Missing command validation | **PASS** |
| **TC-REDIR-12** | `ls >> file.txt` | Repeated operator check | **PASS** |
