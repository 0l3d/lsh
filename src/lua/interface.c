#include "interface.h"
#include <linux/limits.h>
#include <lua5.4/lauxlib.h>
#include <lua5.4/lua.h>
#include <lua5.4/lualib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *new_prompt;

char *promptshell = NULL;
extern char pwd[PATH_MAX];

static lua_State *L = NULL;

void change_prompt() {
  if (promptshell != NULL) {
    free(promptshell);
    promptshell = NULL;
  }

  if (new_prompt != NULL) {
    promptshell = strdup(new_prompt);
  }
}

int set_prompt(lua_State *L) {
  const char *str = luaL_checkstring(L, 1);
  free(new_prompt);
  new_prompt = strdup(str);
  return 0;
}

void shell_update() {
  if (!L)
    return;
  lua_getglobal(L, "update");
  if (lua_isfunction(L, -1)) {
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
      fprintf(stderr, "Lua 'update' function error: %s\n", lua_tostring(L, -1));
      lua_pop(L, 1);
    }
  } else {
    lua_pop(L, 1);
  }
}

void on_cd(const char *path) {
  if (!L)
    return;

  lua_getglobal(L, "on_cd");
  if (lua_isfunction(L, -1)) {
    lua_pushstring(L, path);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
      fprintf(stderr, "Lua 'on_cd' function error: %s\n", lua_tostring(L, -1));
      lua_pop(L, 1);
    }
  } else {
    lua_pop(L, 1); // pop nil
  }
}

// void on_exec() {}

void register_functions() {
  lua_pushcfunction(L, set_prompt);
  lua_setglobal(L, "set_prompt");
}

void updatel_cwd() {
  lua_pushstring(L, pwd);
  lua_setglobal(L, "cwd");
}

void init_lua() {
  L = luaL_newstate();
  luaL_openlibs(L);
  register_functions();
}

void exec_lua(const char *filepath) {
  if (!L) {
    init_lua();
  }
  if (luaL_dofile(L, filepath)) {
    printf("Error: %s\n", lua_tostring(L, -1));
  }
}

int exec_slua(const char *code) {
  if (!L) {
    init_lua();
  }
  if (luaL_dostring(L, code)) {
    printf("Error: %s\n", lua_tostring(L, -1));
    return 1;
  }
  return 0;
}

void close_lua() {
  if (L) {
    lua_close(L);
    L = NULL;
  }
}
