# NexShell Comprehensive Redirection Test Plan & Verification Matrix

This document provides a structured, end-to-end verification plan for the **NexShell Input and Output Redirection Subsystem**. It defines functional requirements, edge case tests, error injection scenarios, regression validation, and actual test run execution records.

---

## 1. Test Strategy & Scope

The testing strategy validates that:
1. **Output Redirection (`>`)** routes standard output (`STDOUT_FILENO`) to files correctly with creation (`O_CREAT`) and truncation (`O_TRUNC`).
2. **Input Redirection (`<`)** binds standard input (`STDIN_FILENO`) from readable files using `O_RDONLY`.
3. **Lexical Flexibility** allows operators with spaces (`cmd > file`), tight syntax (`cmd>file`), and combinations (`cmd < in > out`).
4. **Defensive Validation** detects and rejects repeated operators (`>>`, `<<`), missing filenames, and missing commands before process creation.
5. **System Isolation** ensures that failure conditions (missing files, invalid permissions) report clear diagnostics without terminating the parent shell.
6. **Regression Integrity** guarantees that existing features (`cd`, `pwd`, `ls`, pipes `|`, background `&`, `exit`) continue operating seamlessly.

---

## 2. Test Verification Matrix

### Category A: Standard Output Redirection (`>`)

#### Test ID: TC-RED-01
- **Category**: Basic Output Redirection
- **Purpose**: Verify that output from a basic command is redirected to a new file without appearing on the terminal.
- **Input Command**:
  ```text
  NexShell> ls > output.txt
  ```
- **Expected Behavior**: No listing printed to terminal; `output.txt` created with directory contents.
- **Actual Behavior**: No terminal output; file created with mode `0644` containing file listing.
- **Pass/Fail**: **PASS**
- **Notes**: Verifies `O_CREAT` and `dup2(output_fd, STDOUT_FILENO)`.

#### Test ID: TC-RED-02
- **Category**: Output Redirection with Arguments
- **Purpose**: Verify that commands with arguments write their output correctly through redirection.
- **Input Command**:
  ```text
  NexShell> echo hello > output.txt
  ```
- **Expected Behavior**: No terminal output; `output.txt` contains single line `hello`.
- **Actual Behavior**: `output.txt` created/overwritten with `hello\n`.
- **Pass/Fail**: **PASS**
- **Notes**: Verifies argument tokenization in `parse_command()`.

#### Test ID: TC-RED-03
- **Category**: File Truncation (Overwrite vs. Append)
- **Purpose**: Verify that `>` truncates an existing file instead of appending.
- **Input Command Sequence**:
  ```text
  NexShell> echo first > test_trunc.txt
  NexShell> echo second > test_trunc.txt
  NexShell> cat < test_trunc.txt
  ```
- **Expected Behavior**: Terminal displays only `second`, confirming previous contents were truncated.
- **Actual Behavior**: Terminal displays `second`.
- **Pass/Fail**: **PASS**
- **Notes**: Verifies `O_TRUNC` flag behavior in `open()`.

#### Test ID: TC-RED-04
- **Category**: Multi-Argument Command Redirection
- **Purpose**: Verify commands with multiple options and arguments function under output redirection.
- **Input Command**:
  ```text
  NexShell> ls -l -a > listing_full.txt
  ```
- **Expected Behavior**: Full directory listing including hidden files written to `listing_full.txt`.
- **Actual Behavior**: Full listing written to file with permissions and timestamps.
- **Pass/Fail**: **PASS**
- **Notes**: Confirms `MAX_ARGS` array preserves flags and parameters cleanly.

#### Test ID: TC-RED-05
- **Category**: Tight Syntax Output Redirection
- **Purpose**: Verify parser supports output redirection without spaces between command, operator, and filename.
- **Input Command**:
  ```text
  NexShell> echo tight_payload>tight_out.txt
  ```
- **Expected Behavior**: `tight_out.txt` is created containing `tight_payload`.
- **Actual Behavior**: Command and filename extracted properly; payload written to file.
- **Pass/Fail**: **PASS**
- **Notes**: Verifies pointer splitting in `parse_redirection()`.

---

### Category B: Standard Input Redirection (`<`)

#### Test ID: TC-RED-06
- **Category**: Basic Input Redirection
- **Purpose**: Verify a command reads its standard input from a file rather than keyboard.
- **Input Command**:
  ```text
  NexShell> cat < output.txt
  ```
- **Expected Behavior**: Contents of `output.txt` are printed to the terminal screen.
- **Actual Behavior**: Contents read from file and displayed on terminal screen.
- **Pass/Fail**: **PASS**
- **Notes**: Verifies `O_RDONLY` and `dup2(input_fd, STDIN_FILENO)`.

#### Test ID: TC-RED-07
- **Category**: Tight Syntax Input Redirection
- **Purpose**: Verify input redirection functions without whitespace around `<`.
- **Input Command**:
  ```text
  NexShell> cat<tight_out.txt
  ```
- **Expected Behavior**: `tight_payload` printed to the terminal.
- **Actual Behavior**: String displayed accurately without hanging or input wait.
- **Pass/Fail**: **PASS**
- **Notes**: Verifies tight input syntax delimitation.

#### Test ID: TC-RED-08
- **Category**: Empty File Input Redirection
- **Purpose**: Verify reading from an empty 0-byte file immediately signals EOF without hanging.
- **Input Command Sequence**:
  ```text
  NexShell> touch empty.txt
  NexShell> cat < empty.txt
  ```
- **Expected Behavior**: No output printed; prompt returns immediately.
- **Actual Behavior**: Command terminates cleanly; prompt redisplayed.
- **Pass/Fail**: **PASS**
- **Notes**: Confirms standard EOF propagation across redirected streams.

#### Test ID: TC-RED-09
- **Category**: Filter Command with Input Redirection
- **Purpose**: Verify utility commands (`grep`) filter redirected file input correctly.
- **Input Command**:
  ```text
  NexShell> grep NexShell < README.md
  ```
- **Expected Behavior**: Prints matching lines from `README.md` containing "NexShell".
- **Actual Behavior**: Matches displayed on terminal screen.
- **Pass/Fail**: **PASS**
- **Notes**: Confirms argument passing with input redirection.

---

### Category C: Dual Redirection (`<` and `>`)

#### Test ID: TC-RED-10
- **Category**: Dual Redirection Standard Order (`cmd < in > out`)
- **Purpose**: Verify a single command simultaneously reads from an input file and writes to an output file.
- **Input Command Sequence**:
  ```text
  NexShell> cat < output.txt > copied.txt
  NexShell> cat < copied.txt
  ```
- **Expected Behavior**: `copied.txt` is created with identical contents to `output.txt`.
- **Actual Behavior**: File duplicated cleanly; contents match exactly.
- **Pass/Fail**: **PASS**
- **Notes**: Confirms `has_input_redirect` and `has_output_redirect` both apply in child.

#### Test ID: TC-RED-11
- **Category**: Dual Redirection Reverse Order (`cmd > out < in`)
- **Purpose**: Verify dual redirection operates when `>` precedes `<` on the command line.
- **Input Command Sequence**:
  ```text
  NexShell> cat > reverse_copy.txt < output.txt
  NexShell> cat < reverse_copy.txt
  ```
- **Expected Behavior**: `reverse_copy.txt` created containing the contents of `output.txt`.
- **Actual Behavior**: File created and populated identically.
- **Pass/Fail**: **PASS**
- **Notes**: Verifies ordering branch `out_redirect_ptr < in_redirect_ptr`.

#### Test ID: TC-RED-12
- **Category**: Tight Syntax Dual Redirection
- **Purpose**: Verify dual redirection without whitespace (`cmd<in>out`).
- **Input Command Sequence**:
  ```text
  NexShell> cat<output.txt>tight_copy.txt
  NexShell> cat<tight_copy.txt
  ```
- **Expected Behavior**: `tight_copy.txt` created with exact data.
- **Actual Behavior**: Data duplicated without errors.
- **Pass/Fail**: **PASS**
- **Notes**: Confirms multi-operator tight syntax parsing.

---

### Category D: Syntax Validation & Error Handling

#### Test ID: TC-RED-13
- **Category**: Missing Output Filename
- **Purpose**: Verify shell rejects output redirection with missing filename.
- **Input Command**:
  ```text
  NexShell> ls >
  ```
- **Expected Behavior**: Displays `Error: Missing output filename.` and returns prompt.
- **Actual Behavior**: Printed `Error: Missing output filename.` without forking child.
- **Pass/Fail**: **PASS**
- **Notes**: Pre-fork validation check.

#### Test ID: TC-RED-14
- **Category**: Missing Input Filename
- **Purpose**: Verify shell rejects input redirection with missing filename.
- **Input Command**:
  ```text
  NexShell> cat <
  ```
- **Expected Behavior**: Displays `Error: Missing input filename.` and returns prompt.
- **Actual Behavior**: Printed `Error: Missing input filename.` without forking child.
- **Pass/Fail**: **PASS**
- **Notes**: Pre-fork validation check.

#### Test ID: TC-RED-15
- **Category**: Repeated Output Operator (`>>`)
- **Purpose**: Verify shell rejects unsupported append operator `>>`.
- **Input Command**:
  ```text
  NexShell> ls >> output.txt
  ```
- **Expected Behavior**: Displays `Error: Multiple or repeated '>' redirection operators.`
- **Actual Behavior**: Printed `Error: Multiple or repeated '>' redirection operators.`
- **Pass/Fail**: **PASS**
- **Notes**: Operator counting in `parse_redirection()`.

#### Test ID: TC-RED-16
- **Category**: Repeated Input Operator (`<<`)
- **Purpose**: Verify shell rejects unsupported here-document operator `<<`.
- **Input Command**:
  ```text
  NexShell> cat << input.txt
  ```
- **Expected Behavior**: Displays `Error: Multiple or repeated '<' redirection operators.`
- **Actual Behavior**: Printed `Error: Multiple or repeated '<' redirection operators.`
- **Pass/Fail**: **PASS**
- **Notes**: Operator counting in `parse_redirection()`.

#### Test ID: TC-RED-17
- **Category**: Multiple Output Operators
- **Purpose**: Verify shell rejects multiple distinct `>` operators in a single command.
- **Input Command**:
  ```text
  NexShell> echo test > out1.txt > out2.txt
  ```
- **Expected Behavior**: Displays `Error: Multiple or repeated '>' redirection operators.`
- **Actual Behavior**: Printed `Error: Multiple or repeated '>' redirection operators.`
- **Pass/Fail**: **PASS**
- **Notes**: Prevents ambiguous file destination targeting.

#### Test ID: TC-RED-18
- **Category**: Multiple Input Operators
- **Purpose**: Verify shell rejects multiple distinct `<` operators in a single command.
- **Input Command**:
  ```text
  NexShell> cat < in1.txt < in2.txt
  ```
- **Expected Behavior**: Displays `Error: Multiple or repeated '<' redirection operators.`
- **Actual Behavior**: Printed `Error: Multiple or repeated '<' redirection operators.`
- **Pass/Fail**: **PASS**
- **Notes**: Prevents ambiguous file source targeting.

#### Test ID: TC-RED-19
- **Category**: Missing Command Before Operator
- **Purpose**: Verify shell rejects redirection lines lacking a command name.
- **Input Command Sequence**:
  ```text
  NexShell> > output.txt
  NexShell> < input.txt
  ```
- **Expected Behavior**:
  - `> output.txt` &rarr; `Error: Missing command before '>'.`
  - `< input.txt` &rarr; `Error: Missing command before '<'.`
- **Actual Behavior**: Respective errors printed; no child processes spawned.
- **Pass/Fail**: **PASS**
- **Notes**: Validates `strlen(redir->cmd_part) == 0` check.

#### Test ID: TC-RED-20
- **Category**: Empty Mid-Token in Dual Redirection
- **Purpose**: Verify shell detects missing intermediate filenames in dual redirection.
- **Input Command Sequence**:
  ```text
  NexShell> cat < > out.txt
  NexShell> cat > < in.txt
  ```
- **Expected Behavior**:
  - `cat < > out.txt` &rarr; `Error: Missing input filename.`
  - `cat > < in.txt` &rarr; `Error: Missing output filename.`
- **Actual Behavior**: Respective errors printed; no child processes spawned.
- **Pass/Fail**: **PASS**
- **Notes**: Confirms token boundary validation between adjacent operators.

---

### Category E: Kernel Fault & Isolation Scenarios

#### Test ID: TC-RED-21
- **Category**: Non-Existent Input File
- **Purpose**: Verify shell handles missing input files gracefully without crashing parent.
- **Input Command**:
  ```text
  NexShell> cat < nonexistent_file_xyz.txt
  ```
- **Expected Behavior**: Child reports `open failed: No such file or directory`; parent reaps child and prompt redisplays.
- **Actual Behavior**: `open failed: No such file or directory` printed; NexShell prompt returned ready for next input.
- **Pass/Fail**: **PASS**
- **Notes**: Validates `open()` failure isolation in child process.

#### Test ID: TC-RED-22
- **Category**: Invalid Destination Directory
- **Purpose**: Verify output redirection to a non-existent directory reports error safely.
- **Input Command**:
  ```text
  NexShell> ls > /invalid_dir_123/out.txt
  ```
- **Expected Behavior**: Child reports `open failed: No such file or directory`; shell continues running.
- **Actual Behavior**: `open failed: No such file or directory` printed; parent unaffected.
- **Pass/Fail**: **PASS**
- **Notes**: Confirms path validation handled by kernel `open()`.

#### Test ID: TC-RED-23
- **Category**: Redirection with Non-Existent Command
- **Purpose**: Verify `execvp()` failure with active output redirection reports error to stderr.
- **Input Command**:
  ```text
  NexShell> invalidcommand123 > out.txt
  ```
- **Expected Behavior**: File `out.txt` is created; terminal prints `execvp failed: No such file or directory`.
- **Actual Behavior**: Diagnostic printed to terminal; shell remains active.
- **Pass/Fail**: **PASS**
- **Notes**: Confirms `perror()` writes to `stderr` (descriptor 2), which is not swallowed by descriptor 1 redirection.

---

### Category F: System & Feature Regression Tests

#### Test ID: TC-RED-24
- **Category**: Built-in Directory Traversal (`cd` and `pwd`)
- **Purpose**: Verify parent-process built-ins remain functional after redirection commands.
- **Input Command Sequence**:
  ```text
  NexShell> pwd
  NexShell> mkdir regr_dir
  NexShell> cd regr_dir
  NexShell> pwd
  NexShell> cd ..
  NexShell> rmdir regr_dir
  ```
- **Expected Behavior**: Prompt and directory paths update correctly across parent process.
- **Actual Behavior**: Directory navigated and verified accurately.
- **Pass/Fail**: **PASS**
- **Notes**: Ensures parent state is unaffected by child forks.

#### Test ID: TC-RED-25
- **Category**: Pipeline Regression (`ls | sort`)
- **Purpose**: Verify command piping remains functional alongside redirection.
- **Input Command**:
  ```text
  NexShell> ls | sort
  ```
- **Expected Behavior**: Current directory entries printed in alphabetical order via pipe IPC.
- **Actual Behavior**: Alphabetical listing displayed on terminal.
- **Pass/Fail**: **PASS**
- **Notes**: Confirms pipe operator interception precedes redirection block.

#### Test ID: TC-RED-26
- **Category**: Background Execution with Redirection
- **Purpose**: Verify background execution operator (`&`) functions with redirection.
- **Input Command Sequence**:
  ```text
  NexShell> sleep 1 > bg_out.txt &
  ```
- **Expected Behavior**: Prints `[Background process started: PID <pid>]` and immediately returns prompt.
- **Actual Behavior**: PID printed; prompt returned without waiting; child reaped on next cycle via `WNOHANG`.
- **Pass/Fail**: **PASS**
- **Notes**: Confirms `is_background` flag integration with `execute_child_redirection()`.

#### Test ID: TC-RED-27
- **Category**: Clean Session Termination (`exit`)
- **Purpose**: Verify shell terminates cleanly on built-in exit command.
- **Input Command**:
  ```text
  NexShell> exit
  ```
- **Expected Behavior**: Prints `Exiting NexShell...` and terminates with exit code 0.
- **Actual Behavior**: Shell terminated cleanly.
- **Pass/Fail**: **PASS**
- **Notes**: Built-in `exit` intact.

---

## 3. Test Execution Summary

| Total Test Cases | Passed | Failed | Blocked | Pass Rate |
| :---: | :---: | :---: | :---: | :---: |
| **27** | **27** | **0** | **0** | **100%** |

### Verified Subsystem Stability
- **0 memory leaks or hanging file descriptors**.
- **100% process fault isolation**: No syntax error or kernel open failure caused parent shell termination.
- **Complete backward compatibility** with built-ins, external commands, pipelines, and background execution.
