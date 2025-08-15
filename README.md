# LSH

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
git clone https://github.com/0l3d/lsh.git
cd lsh/
make
./lsh
```

## Usage

- **Default Prompt:**

```lua
PATH -> <command>
```

- **Execute Lua Script:**  
  Syntax 1 (run with lshl)

```lua
./script.lshl
```

Syntax 2 (run with lua)

```lua
dofile "script.lshl"
```

lsh also supports lua syntax like:

```lua
print "hello world"
hello=1; if hello == 1 then print "hello world" end
```

also you can use lua libs via luarocks, for example:

```lua
-- luarocks install luasocket --local --lua-version=5.4
local socket = require("socket")
print("Current time from socket:", socket.gettime())
```

## Planned Features

- Smarter history support.
- TAB Completion.

# LICENSE

This project is licensed under the **GPL-3.0 License**.

---

Created By **0l3d**
