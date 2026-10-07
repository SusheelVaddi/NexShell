# NexShell Redirection Test Suite & Reference Directory

This directory provides interactive testing references and validation command suites for the **NexShell Input and Output Redirection subsystem**.

---

## Directory Structure

- **[`manual_test_commands.txt`](manual_test_commands.txt)**: A raw, copy-pasteable command script containing 25+ verified test commands spanning normal syntax, tight syntax, dual redirection, and error edge cases.
- **[`edge_case_matrix.md`](edge_case_matrix.md)**: A quick-reference lookup table summarizing syntax patterns, expected output streams, and error diagnostics.

---

## How to Run the Manual Test Commands

To interactively execute the entire redirection test suite against a compiled NexShell binary:

```bash
# 1. Compile NexShell with all warnings enabled:
gcc -Wall -Wextra main.c -o nexshell

# 2. Pipe the manual test commands directly into NexShell:
./nexshell < tests/redirection/manual_test_commands.txt

# Or execute interactively line-by-line:
./nexshell
```

---

## Testing Coverage

The reference suite exercises:
1. **Output Redirection (`>`)**: Creation, truncation, multi-argument commands.
2. **Input Redirection (`<`)**: Reading existing files, handling empty files.
3. **Tight Syntax (`cmd>file`, `cmd<file`)**: Whitespace-free operator handling.
4. **Dual Redirection (`cmd < in > out`, `cmd > out < in`, `cmd<in>out`)**: Two-way file filtering.
5. **Defensive Syntax Rejection**: Missing filenames, missing commands, repeated operators (`>>`, `<<`).
6. **Kernel Fault Isolation**: Non-existent files, non-existent directories, invalid commands.
7. **Subsystem Regression**: Pipelines (`|`), background execution (`&`), built-in `cd`, `pwd`, `exit`.
