#include "../lshlib.h"
#include <lua5.4/lauxlib.h>
#include <lua5.4/lua.h>
#include <lua5.4/lualib.h>
#include <stdio.h>

void exec_lua(const char *filepath) {
  lua_State *L = luaL_newstate();
  luaL_openlibs(L);
  if (luaL_dofile(L, filepath)) {
    printf("Error: %s\n", lua_tostring(L, -1));
  }

  lua_close(L);
}
