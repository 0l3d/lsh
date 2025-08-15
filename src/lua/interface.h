#include <linux/limits.h>
void exec_lua(const char *filepath);
int exec_slua(const char *code);
void change_prompt();
void init_lua();
void close_lua();
void updatel_cwd();
void shell_update();
void on_cd(const char *path);

extern int lua_running;
extern char *promptshell;
extern char pwd[PATH_MAX];
