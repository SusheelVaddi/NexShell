# Redirection Testing Scenarios and Verification Guide

This document outlines practical testing scenarios for verifying standard input (`<`) and standard output (`>`) redirection in **NexShell**. Each scenario details the command, expected terminal behavior, filesystem results, and the underlying POSIX kernel mechanisms being verified.

---

## 1. Overview of Redirection Testing

Testing input and output redirection in NexShell ensures that:
- Standard output (`STDOUT_FILENO`) is cleanly routed to target files without leaking onto the terminal screen.
- Standard input (`STDIN_FILENO`) reads directly from source files without blocking for keyboard input.
- File descriptors are created, duplicated, and closed cleanly to prevent descriptor exhaustion.
- File creation (`O_CREAT`) and truncation (`O_TRUNC`) work accurately without appending stale data.
- Operator parsing works reliably with both standard whitespace separation and tight syntax without spaces.
- Non-existent files or permission errors are handled gracefully without terminating the parent shell.

---

## 2. Core Redirection Test Scenarios

### Scenario 1: Basic Output Redirection (`ls > output.txt`)
- **Command**:
  ```bash
  NexShell> ls > output.txt
  ```
- **Expected Behavior**:
  - No directory listing is printed to the terminal screen.
  - NexShell displays the prompt again immediately after execution finishes.
  - A file named `output.txt` is created in the current working directory containing the list of directory entries.
- **What Feature is Being Verified**:
  - **Operator Detection**: Confirms `>` is identified and stripped from the argument list passed to `execvp()`.
  - **File Creation & Permissions**: Confirms `open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644)` successfully creates the file.
  - **Stream Binding**: Confirms `dup2(output_fd, STDOUT_FILENO)` binds file descriptor `1` to the file before `ls` runs.
  - **Foreground Synchronization**: Confirms parent shell waits for child termination via `waitpid()`.

---

### Scenario 2: Input Redirection (`cat < output.txt`)
- **Command**:
  ```bash
  NexShell> cat < output.txt
  ```
- **Expected Behavior**:
  - The contents of `output.txt` (the directory listing created in Scenario 1) are printed directly to the terminal screen.
  - The `cat` command completes execution without hanging or prompting for user keyboard input.
- **What Feature is Being Verified**:
  - **Operator Detection**: Confirms `<` is identified and stripped from the argument array.
  - **Read-Only Descriptor Allocation**: Confirms `open("output.txt", O_RDONLY)` opens the target file in read-only mode.
  - **Standard Input Binding**: Confirms `dup2(input_fd, STDIN_FILENO)` points descriptor `0` to the file.
  - **EOF Signalling**: Confirms utilities reading from `stdin` receive End-of-File (EOF) correctly when reading completes.

---

### Scenario 3: Output Redirection with Echo (`echo hello > output.txt`)
- **Command**:
  ```bash
  NexShell> echo hello > output.txt
  ```
- **Expected Behavior**:
  - The string `"hello"` is NOT displayed on the terminal screen.
  - NexShell returns immediately to the prompt.
  - The existing `output.txt` file is overwritten (truncated from its previous directory listing) so that its sole content is `hello\n`.
- **What Feature is Being Verified**:
  - **Multi-Token Command Parsing**: Verifies that multi-token commands (`echo` + argument `"hello"`) are properly parsed prior to `>`.
  - **File Truncation (`O_TRUNC`)**: Confirms that previous contents of `output.txt` are completely overwritten rather than appended.

---

### Scenario 4: Reading the Redirected File (`cat < output.txt`)
- **Command**:
  ```bash
  NexShell> cat < output.txt
  ```
- **Expected Behavior**:
  - The terminal prints:
    ```text
    hello
    ```
- **What Feature is Being Verified**:
  - **Data Integrity**: Confirms that the exact string written by `echo` in Scenario 3 was recorded and read back without corruption or extra null bytes.
  - **Repeatability**: Confirms repeatable input redirection across successive shell commands.

---

### Scenario 5: Tight Syntax Output Redirection (`ls>output.txt`)
- **Command**:
  ```bash
  NexShell> ls>output.txt
  ```
- **Expected Behavior**:
  - Executes identically to `ls > output.txt`.
  - No text appears on the terminal; `output.txt` is overwritten with the fresh directory listing.
- **What Feature is Being Verified**:
  - **Whitespace-Free Parsing**: Verifies that NexShell's parser does not depend on whitespace surrounding the `>` operator.
  - **String Splitting Accuracy**: Confirms correct pointer-based string splitting when commands and filenames are directly adjacent to operators.

---

### Scenario 6: Tight Syntax Input Redirection (`cat<output.txt`)
- **Command**:
  ```bash
  NexShell> cat<output.txt
  ```
- **Expected Behavior**:
  - The contents of `output.txt` are printed to the terminal without requiring spaces around `<`.
- **What Feature is Being Verified**:
  - **Tight Input Delimitation**: Verifies that NexShell's parser handles tight input syntax without space delimiters.
  - **Execution Robustness**: Confirms clean tokenization and execution of `cat` with standard input redirected from `output.txt`.

---

## 3. Edge Case & Stability Test Scenarios

### Scenario 7: Overwrite Verification (Truncate vs. Append)
- **Command Sequence**:
  ```bash
  NexShell> echo first > test.txt
  NexShell> cat < test.txt
  NexShell> echo second > test.txt
  NexShell> cat < test.txt
  ```
- **Expected Behavior**:
  - The first `cat` prints `first`.
  - The second `cat` prints `second` (confirming `first` was completely overwritten, not appended).
- **What Feature is Being Verified**:
  - Verifies that `O_TRUNC` resets file size to 0 bytes on each output redirection open call.

---

### Scenario 8: Non-Existent File Input Error Handling
- **Command**:
  ```bash
  NexShell> cat < does_not_exist.txt
  ```
- **Expected Behavior**:
  - NexShell outputs an error message:
    ```text
    open failed: No such file or directory
    ```
  - The shell does NOT crash and does NOT terminate.
  - NexShell immediately displays the prompt `NexShell> ` ready for the next command.
- **What Feature is Being Verified**:
  - **Child Process Isolation**: Verifies that `open()` failure causes the child process to call `perror` and `exit(1)`.
  - **Parent Shell Resilience**: The parent shell safely reaps the failed child via `waitpid()` and continues running.

---

### Scenario 9: Missing Arguments Validation
- **Commands**:
  ```bash
  NexShell> ls >
  NexShell> cat <
  NexShell> > output.txt
  NexShell> < input.txt
  ```
- **Expected Behavior**:
  - `ls >` prints `Error: Missing output filename.`
  - `cat <` prints `Error: Missing input filename.`
  - `> output.txt` prints `Error: Missing command before '>'.`
  - `< input.txt` prints `Error: Missing command before '<'.`
- **What Feature is Being Verified**:
  - Pre-fork input validation to prevent spawning invalid child processes.

---

### Scenario 10: Dual Input and Output Redirection
- **Command Sequence**:
  ```bash
  NexShell> echo "sample payload" > source.txt
  NexShell> cat < source.txt > target.txt
  NexShell> cat < target.txt
  ```
- **Expected Behavior**:
  - `cat < target.txt` prints `"sample payload"`.
- **What Feature is Being Verified**:
  - Verifies simultaneous application of `dup2(input_fd, STDIN_FILENO)` and `dup2(output_fd, STDOUT_FILENO)` within a single child process.

---

## 4. Test Artifact Cleanup

All test files created during interactive validation (such as `output.txt`, `test.txt`, `source.txt`, and `target.txt`) are temporary artifacts and must be removed prior to git commits:

```bash
rm -f output.txt test.txt source.txt target.txt
```
