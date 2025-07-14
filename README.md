# MINSHELL
Minimalist, Fast and Simple shell in C.

## Features

- **Command History**  
  Navigate through previously entered commands using the ↑ arrow key and 󰁅 arrow key.

- **Path-Aware Prompt**  
  Displays the current working directory dynamically as part of the prompt.

- **Built-in `cd` and `exit` commands**  
  Easily change directories or exit the shell like in any standard terminal.

- **Raw mode input handling**  
  Captures keystrokes directly for responsive editing and key navigation.

## Installation

```bash
gcc minshell.c -o minshell
./minshell
```

##  Planned Features
-  Lua Support for Scripting and Configuration.
-  Smarter history support.
-  TAB Completion.
-  fork()/execvp() calls instead of system().

# LICENSE
This project is licensed under the **GPL-3.0 License**.

--- 

Created By **0l3d**
