# NexShell Redirection Edge Case Reference Matrix

This document provides a quick reference table of syntax edge cases, operator placement variations, kernel error conditions, and their observed responses in **NexShell**.

---

## 1. Syntax & Grammar Matrix

| Input Pattern | Example Command | Expected Shell Handling | Error Output (if any) |
| :--- | :--- | :--- | :--- |
| **Normal Output** | `ls > out.txt` | File created/truncated, `stdout` routed to file | None |
| **Normal Input** | `cat < in.txt` | Reads from `in.txt`, outputs to terminal | None |
| **Tight Output** | `ls>out.txt` | String split at `>`, file created | None |
| **Tight Input** | `cat<in.txt` | String split at `<`, file read | None |
| **Standard Dual** | `cat < in.txt > out.txt` | Reads `in.txt`, writes to `out.txt` | None |
| **Reverse Dual** | `cat > out.txt < in.txt` | Reads `in.txt`, writes to `out.txt` | None |
| **Tight Dual** | `cat<in.txt>out.txt` | Reads `in.txt`, writes to `out.txt` | None |
| **Missing Output Filename** | `ls >` | Rejected before `fork()` | `Error: Missing output filename.` |
| **Missing Input Filename** | `cat <` | Rejected before `fork()` | `Error: Missing input filename.` |
| **Repeated `>`** | `ls >> out.txt` | Rejected before `fork()` | `Error: Multiple or repeated '>' redirection operators.` |
| **Repeated `<`** | `cat << in.txt` | Rejected before `fork()` | `Error: Multiple or repeated '<' redirection operators.` |
| **Multiple `>` Files** | `echo a > f1 > f2` | Rejected before `fork()` | `Error: Multiple or repeated '>' redirection operators.` |
| **Multiple `<` Files** | `cat < f1 < f2` | Rejected before `fork()` | `Error: Multiple or repeated '<' redirection operators.` |
| **Missing Command (`>`)** | `> out.txt` | Rejected before `fork()` | `Error: Missing command before '>'.` |
| **Missing Command (`<`)** | `< in.txt` | Rejected before `fork()` | `Error: Missing command before '<'.` |
| **Adjacent Dual (`< >`)** | `cat < > out.txt` | Rejected before `fork()` | `Error: Missing input filename.` |
| **Adjacent Dual (`> <`)** | `cat > < in.txt` | Rejected before `fork()` | `Error: Missing output filename.` |

---

## 2. Runtime & Kernel Error Matrix

| Fault Condition | Example Command | Handling Process | Kernel Return | Error Message Displayed | Shell Survives? |
| :--- | :--- | :--- | :--- | :--- | :---: |
| **Missing Input File** | `cat < missing.txt` | Child Process | `open()` returns `-1` (`ENOENT`) | `open failed: No such file or directory` | **YES** |
| **Protected Input File** | `cat < no_read.txt` | Child Process | `open()` returns `-1` (`EACCES`) | `open failed: Permission denied` | **YES** |
| **Invalid Output Directory** | `ls > /bad/out.txt` | Child Process | `open()` returns `-1` (`ENOENT`) | `open failed: No such file or directory` | **YES** |
| **Invalid Command Binary** | `badcmd > out.txt` | Child Process | `execvp()` returns `-1` (`ENOENT`) | `execvp failed: No such file or directory` | **YES** |
| **Parent Pipeline Interference** | `ls \| sort > out.txt` | Parent Shell | Pipe parser intercepts `\|` first | Pipeline executes; right child handles redirection | **YES** |
| **Background Redirection** | `sleep 2 > out.txt &` | Parent/Child | `is_background` detected | Shell prints PID and returns prompt immediately | **YES** |
