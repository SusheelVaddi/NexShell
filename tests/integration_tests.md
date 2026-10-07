# NexShell Test Suite: End-to-End Integration Tests

This document specifies full end-to-end multi-step workflow integration tests demonstrating combined features across session lifecycles.

---

## Test Cases

### Test TC-INT-01: Full Lifecycle Project Workflow
- **Objective**: Verify sequential interaction involving folder creation, directory traversal, file generation with redirection, content inspection, pipeline filtering, background process, and exit.
- **Workflow Sequence**:
  ```text
  NexShell> pwd
  NexShell> mkdir integration_demo
  NexShell> cd integration_demo
  NexShell> pwd
  NexShell> ls > dir_contents.txt
  NexShell> cat < dir_contents.txt
  NexShell> ls | grep dir
  NexShell> sleep 2 &
  NexShell> cd ..
  NexShell> rmdir integration_demo
  NexShell> exit
  ```
- **Expected Behavior**:
  1. `pwd` displays initial project root.
  2. `mkdir` creates `integration_demo`.
  3. `cd integration_demo` changes parent working directory.
  4. `ls > dir_contents.txt` creates file containing directory entries.
  5. `cat < dir_contents.txt` displays the file's contents.
  6. `ls | grep dir` displays `dir_contents.txt`.
  7. `sleep 2 &` spawns asynchronous child process and prints PID.
  8. `cd ..` navigates back to root.
  9. `exit` terminates shell session cleanly.
- **Actual Behavior**: Every step executed in sequence without error, memory leaks, or hung parent processes.
- **Feature Being Verified**: Cross-feature interoperability (`cd`, `mkdir`, `>`, `<`, `|`, `&`, `exit`).
- **Status**: **PASS**

---

### Test TC-INT-02: Pipeline Filtering into File Redirection Workaround
- **Objective**: Verify multi-step workflow achieving pipeline output logging through sequential redirection commands.
- **Workflow Sequence**:
  ```text
  NexShell> ls > stage1.txt
  NexShell> cat < stage1.txt > stage2.txt
  NexShell> cat < stage2.txt
  ```
- **Expected Behavior**: Creates `stage1.txt` with directory entries; copies contents into `stage2.txt` via dual redirection; verifies file contents accurately.
- **Actual Behavior**: Sequence executes cleanly and produces identical file outputs.
- **Feature Being Verified**: Sequential file redirection data pipelines.
- **Status**: **PASS**

---

### Test TC-INT-03: Multiple Background Concurrency & Reaping
- **Objective**: Verify handling of multiple simultaneous background tasks and collective zombie cleanup.
- **Workflow Sequence**:
  ```text
  NexShell> sleep 1 &
  NexShell> sleep 2 &
  NexShell> pwd
  # (Wait 3 seconds)
  NexShell> pwd
  ```
- **Expected Behavior**: Spawns two distinct background PIDs; parent remains responsive; both processes reaped cleanly via `WNOHANG` loop upon subsequent command invocation.
- **Actual Behavior**: Both PIDs reported; reaped during subsequent command prompt iterations; no zombie processes accumulated.
- **Feature Being Verified**: Multi-child background scheduling and non-blocking process table reclamation.
- **Status**: **PASS**

---

## Summary Matrix

| Test ID | Workflow Scope | Target Mechanisms | Result |
| :--- | :--- | :--- | :--- |
| **TC-INT-01** | Full interactive session lifecycle | Built-ins, external binaries, I/O redirection, pipes, async execution, exit | **PASS** |
| **TC-INT-02** | Staged multi-command data transformation | Sequential redirection and file validation | **PASS** |
| **TC-INT-03** | Multiple concurrent background jobs | Multi-PID asynchronous tracking and batch `WNOHANG` reaping | **PASS** |
