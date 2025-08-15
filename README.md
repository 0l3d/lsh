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

also you can use lua libs installed via luarocks, for example:

```lua
-- luarocks install luasocket --local --lua-version=5.4
local socket = require("socket")
print("Current time from socket:", socket.gettime())
```

## Planned Features

- Smarter history support.
- TAB Completion.

### Notes

i saw a bug in shell, if you type a normal command like `ls` then exit, it works perfectly, but if you type lua code like `print "hello world"` then exit, it doesnt work unless i type it twice, i thought about it for a couple of hours because there s was no explanation for this situation, i wrote some debugs and when i typed an exit, debugs worked, i said wtf? you re getting an exit signal and youre not exiting, why? and i looked at pids and i saw it, lua dostring opens an extra pid for process and doesnt closes it, when i type an exit, im ending lua process, shell is still alive, the bug fix is here:

```
void lua_syntax_sup(char *string) {
  if (exec_slua(string) != 0) {
    fprintf(stderr, "Command not found or Lua failed:\n%s\n", string);
  }
  exit(0); // i added this line, if you run a lua process directly in the shell, it will exit properly after finishing.
}
```

# LICENSE

This project is licensed under the **GPL-3.0 License**.

---

Created By **0l3d**
