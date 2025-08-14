#include "libhalloc/halloc.h"
#include "lua/interface.h"
#include "lua/mainlib.h"
#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/limits.h>
#include <lua5.4/lauxlib.h>
#include <lua5.4/lua.h>
#include <lua5.4/lualib.h>
#include <pty.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#define MAX_TOKENS 200
#define MAX_SUBS 100

struct termios orig_set;

void restore_terminal() { tcsetattr(STDIN_FILENO, TCSANOW, &orig_set); }

void set_raw_terminal() {
  struct termios raw;
  tcgetattr(STDIN_FILENO, &raw);
  raw.c_lflag &= ~(ICANON | ECHO | ISIG);
  tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

extern char *promptshell;
char pwd[PATH_MAX];

void prompt(char *path) {
  shell_update();
  change_prompt();
  if (promptshell != NULL) {
    printf("%s", promptshell);
  } else {
    printf("%s -> ", path);
  }
  fflush(stdout);
}

void lua_syntax_sup(char *string) {
  if (exec_slua(string) != 0) {
    fprintf(stderr, "Command not found or Lua failed:\n%s\n", string);
  }
}

int background_process = 0;

int command_lexer(char *bufin, char *bufout[], int max_count) {
  int token_count = 0;
  const char *p = bufin;
  while (*p != '\0') {
    if (p[0] == '>' && p[1] == '(') {
      char *sub = halloc_safe(3);
      sub[0] = '>';
      sub[1] = '(';
      sub[2] = '\0';
      bufout[token_count++] = sub;
      p += 2;
    } else if (p[0] == '<' && p[1] == '(') {
      char *sub = halloc_safe(3);
      sub[0] = '<';
      sub[1] = '(';
      sub[2] = '\0';
      bufout[token_count++] = sub;
      p += 2;
    } else if (p[0] == ')') {
      char *subr = halloc_safe(2);
      subr[0] = ')';
      subr[1] = '\0';
      bufout[token_count++] = subr;
      p += 1;
    } else if (*p == '&') {
      char *sub = halloc_safe(2);
      sub[0] = '&';
      sub[1] = '\0';
      bufout[token_count++] = sub;
      p++;
    } else if (isspace(*p)) {
      p++;
      continue;
    } else if (*p == '\0')
      break;
    else if (*p == '>' || *p == '<' || *p == '|') {
      char *sym = halloc_safe(2);
      sym[0] = *p;
      sym[1] = '\0';
      bufout[token_count] = sym;
      token_count++;
      p++;
    } else if (*p == '"' || *p == '\'') {
      char in_string = *p;
      const char *string_start = ++p;
      while (*p && *p != in_string)
        p++;
      int stringlen = p - string_start;
      char *in_string_tokens = halloc_safe(stringlen + 1);
      strncpy(in_string_tokens, string_start, stringlen);
      in_string_tokens[stringlen] = '\0';
      bufout[token_count++] = in_string_tokens;
      if (*p)
        p++;
    } else {
      const char *word_start = p;
      while (*p && !isspace(*p) && *p != '>' && *p != '<' && *p != '|' &&
             *p != ')')
        p++;
      int word_len = p - word_start;
      char *word = halloc_safe(word_len + 1);
      strncpy(word, word_start, word_len);
      word[word_len] = '\0';
      bufout[token_count++] = word;
    }
    if (token_count >= max_count)
      break;
  }
  bufout[token_count] = NULL;
  return token_count;
}

void free_tokens(char *tokens[], int count) {
  for (int i = 0; i < count; ++i) {
    hfree_safe(tokens[i]);
  }
}

int basic_pipe(char *tokens[], int i) {
  int saved_stdout = -1;
  if (strcmp(tokens[i], "<") == 0) {
    int fd = open(tokens[i + 1], O_RDONLY);
    if (fd == -1) {
      perror("file open failed");
      return -1;
    }
    dup2(fd, STDIN_FILENO);
    close(fd);
    tokens[i] = NULL;
    return 1;
  } else if (strcmp(tokens[i], ">") == 0) {
    int fd = open(tokens[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
      perror("file open failed");
      return -1;
    }
    dup2(fd, STDOUT_FILENO);
    close(fd);
    tokens[i] = NULL;
    return 1;
  } else if (strcmp(tokens[i], "&") == 0) {
    background_process = 1;
    tokens[i] = NULL;
    return 1;
  }
  return 0;
}

int itscript;
int lua_parser(char **tokens, int i) {
  if (tokens[i][0] == '.' && tokens[i][1] == '/') {
    char *dotcontrol = strrchr(tokens[i], '.');
    if (dotcontrol && strcmp(dotcontrol, ".lshl") == 0) {
      restore_terminal();
      exec_lua(tokens[i]);
      set_raw_terminal();
      prompt(pwd);
    } else {
      return -1;
    }
  } else {
    return -1;
  }
  return 0;
}

void command_parser(char *command) {
  background_process = 0;

  char *tokens[MAX_TOKENS];

  // PIPE HANDLING
  int special_proc = 0;
  int its_pipe = 0;
  int c_pipe = 0;
  int c_pipe_pos[MAX_SUBS];
  // PIPE HANDLING

  int tokenc = command_lexer(command, tokens, MAX_TOKENS);

  for (int i = 0; i < tokenc; i++) {
    if (strcmp(tokens[i], "<") == 0 || strcmp(tokens[i], ">") == 0) {
      special_proc = 1;
      break;
    }
  }

  for (int i = 0; i < tokenc; i++) {
    if (strcmp(tokens[i], "|") == 0) {
      its_pipe = 1;
      c_pipe_pos[c_pipe++] = i;
    }
  }

  if (its_pipe) {
    int start = 0;
    int prev_fd;
    for (int i = 0; i <= c_pipe; i++) {
      int end = (i < c_pipe) ? c_pipe_pos[i] : tokenc;
      int fd[2];
      char *pipetok[MAX_TOKENS];
      if (pipe(fd) < 0)
        exit(1);

      pid_t pid = fork();
      if (pid == -1) {
        perror("fork is failed");
        break;
      }
      if (pid == 0) {
        int pipetok_i = 0;
        for (int k = start; k < end; k++) {
          pipetok[pipetok_i++] = tokens[k];
        }
        pipetok[pipetok_i] = NULL;
        if (start == 0) {
          dup2(fd[1], STDOUT_FILENO);
        } else if (end == tokenc) {
          dup2(prev_fd, STDIN_FILENO);
        } else {
          dup2(prev_fd, STDIN_FILENO);
          dup2(fd[1], STDOUT_FILENO);
        }
        close(fd[0]);
        for (int j = 0; pipetok[j] != NULL; j++) {
          if (strcmp(pipetok[j], "<") == 0 || strcmp(pipetok[j], ">") == 0) {
            if (basic_pipe(pipetok, j) == -1) {
              exit(1);
            }
          }
        }
        if (lua_parser(pipetok, 0) != 0) {
          restore_terminal();
          execvp(pipetok[0], pipetok);
          lua_syntax_sup(command);
          exit(0);
        }
      } else {
        close(fd[1]);
        if (start != 0) {
          close(prev_fd);
        }
        prev_fd = fd[0];
        if (background_process == 0) {
          waitpid(pid, NULL, 0);
        } else {
          printf("PID: [%d]", pid);
        }
        set_raw_terminal();
      }
      start = end + 1;
    }
  }

  if (special_proc && !its_pipe) {
    pid_t pid = fork();
    if (pid == 0) {
      for (int i = 0; i < tokenc; i++) {
        basic_pipe(tokens, i);
      }
      if (lua_parser(tokens, 0) != 0) {
        restore_terminal();
        execvp(tokens[0], tokens);
        lua_syntax_sup(command);
        exit(1);
      }
    } else {
      if (background_process == 0) {
        waitpid(pid, NULL, 0);
      } else {
        printf("PID: [%d]", pid);
      }
      set_raw_terminal();
    }
  }

  if (!its_pipe && !special_proc) {
    for (int i = 0; i < tokenc; ++i) {
      basic_pipe(tokens, i);
    }
    if (lua_parser(tokens, 0) != 0) {
      pid_t pid;

      pid = fork();
      if (pid == -1) {
        fprintf(stderr, "forkpty is failed");
        return;
      }

      if (pid == 0) {
        restore_terminal();
        if (execvp(tokens[0], tokens) == -1) {
          lua_syntax_sup(command);
          return;
        } else {
          printf("command is not executed");
        }
      } else {
        if (background_process == 0) {
          waitpid(pid, NULL, 0);
        } else {
          printf("PID: [%d]", pid);
        }
        set_raw_terminal();
      }
    }
  }

  free_tokens(tokens, tokenc);
}

void interface() {
  if (tcgetattr(STDIN_FILENO, &orig_set) == -1) {
    perror("tcgetattr failed");
    return;
  }
  set_raw_terminal();

  int stdout_backup = dup(STDOUT_FILENO);

  if (getcwd(pwd, sizeof(pwd)) == NULL) {
    perror("pwd error");
  } else {
    updatel_cwd();
  }
  dup2(stdout_backup, STDOUT_FILENO);
  prompt(pwd);
  char *history[100] = {0};
  char command[256] = {0};
  unsigned int command_index = 0;
  char buffer[3] = {0};
  int history_i = 0;
  int history_cur = -1;
  while (1) {
    ssize_t n = read(STDIN_FILENO, buffer, 1);
    if (n <= 0)
      continue;
    if (buffer[0] == '\n' || buffer[0] == '\r') { // ENTER IMPLEMENTATION
      printf("\n");
      fflush(stdout);
      history[history_i++] = strdup(command);
      history_cur = history_i;

      if (strcmp(command, "exit") == 0) {
        restore_terminal();
        break;
      } else if (strncmp(command, "cd ", 3) == 0) { // CD COMMAND
        char *path = command + 3;
        if (chdir(path) != 0) {
          perror("cd failed");
        }
        command_index = 0;
        memset(command, 0, sizeof(command));
        if (getcwd(pwd, sizeof(pwd)) == NULL) {
          perror("pwd error");
        } else {
          updatel_cwd();
          on_cd(pwd);
        }
        dup2(stdout_backup, STDOUT_FILENO);
        prompt(pwd);
        continue;
      } else {
        command_parser(command);
        command_index = 0;
        memset(command, 0, sizeof(command));
        dup2(stdout_backup, STDOUT_FILENO);
        prompt(pwd);
        continue;
      }
    } else if (buffer[0] == 8 || buffer[0] == 127) { // BACKSPACE IMPLEMETATION
      if (command_index > 0) {
        command_index--;
        command[command_index] = '\0';
        printf("\b \b");
        fflush(stdout);
      }
      continue;
    }
    // ARROW KEYS
    if (buffer[0] == 0x1B) {
      n = read(STDIN_FILENO, buffer + 1, 2);
      if (n < 0) {
        perror("read");
      }
      if (buffer[1] == '[') {
        switch (buffer[2]) {
        case 'A': // HISTORY IMPLEMENTATION UP ARROW
          if (history_cur > 0) {
            history_cur--;
            printf("\33[2K\r");
            prompt(pwd);
            strcpy(command, history[history_cur]);
            command_index = strlen(command);
            printf("%s", command);
            fflush(stdout);
            break;
          }
          break;
        case 'D':
          command_index--;
          printf("\x1b[D");
          fflush(stdout);
          break;

        case 'C':
          command_index++;
          printf("\x1b[C");
          fflush(stdout);
          break;
        case 'B': // HISTORY IMPLEMENTATION DOWN ARROW
          if (history_cur + 1 < history_i && history[history_cur + 1]) {
            history_cur++;
            printf("\33[2K\r");
            prompt(pwd);
            strcpy(command, history[history_cur]);
            command_index = strlen(command);
            printf("%s", command);
            fflush(stdout);
            break;
          } else {
            break;
          }
          continue;
        }
      }
      memset(&buffer, 0, sizeof(buffer));
    }
    if (command_index < 255) {
      command[command_index++] = buffer[0];
      command[command_index] = '\0';
      printf("%c", buffer[0]);
      fflush(stdout);
    }
  }
  restore_terminal();

  for (int i = 0; i < history_i; ++i) {
    free(history[i]);
  }
}

int main() {
  init_lua();
  exec_slua(mainlibc);
  interface();
  close_lua();
  free(promptshell);
  return 0;
}
