#include <limits.h>
#include <pty.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>
// gcc minshell.c -I/usr/include/lua5.4 -L/usr/lib/ -Wl,-Bstatic -llua5.4 -Wl,-Bdynamic -lc -lm -o minshell


typedef struct {
  char* command;
  char* args[100];
  int arg_count;
} Command;

struct termios orig_set, new_set;

void prompt(char *path) {
  printf("%s -> ", path);
  fflush(stdout);
}

void process(char* command, char *output, size_t size) {
  int amaster[2];
  pid_t pid;
  int nbytes;

  pid = forkpty(amaster, NULL, NULL, NULL);
  if (pid == -1) {
    fprintf(stderr, "forkpty is failed");
    return;
  }
  

  if (pid == 0) {
    if (execlp("ls", "ls", NULL) == -1) {
      perror("Command execution failed: ");
      return;
    } else {
      printf("command is not executed");
    }
  } else {
    size_t total = 0;
    
    while ((nbytes = read(amaster[0], output + total, size - total - 1)) > 0) {
      total += nbytes;
      printf("%s", output);
    }
    output[total] = '\0';
    waitpid(pid, NULL, 0);
  }
}

int command_lexer(char* bufin, char* bufout[], int max_count) {
  int token_count = 0;
  const char* p = bufin;
  while (*p != '\0') {
    while(isspace(*p)) p++;
    if (*p == '\0') break;

    // PARSER
    if (*p == '>' || *p == '<' || *p == '|') {
      char* sym = malloc(2);
      sym[0] = *p;
      sym[1] = '\0';
      bufout[token_count] = sym;
      token_count++;
      p++;
    } else if (*p == '"' || *p == '\'') {
      char in_string = *p;
      const char* string_start = ++p;
      while (*p && *p != in_string) p++;
      int stringlen = p - string_start;
      char* in_string_tokens = malloc(stringlen + 1);
      strncpy(in_string_tokens, string_start, stringlen);
      in_string_tokens[stringlen] = '\0';
      bufout[token_count++] = in_string_tokens;
      if (*p) p++;
    } else {
      const char* word_start = p;
      while (*p && !isspace(*p) && *p != '>' && *p != '<' && *p != '|') p++;
      int word_len = p - word_start;
      char *word = malloc(word_len + 1);
      strncpy(word, word_start, word_len);
      word[word_len] = '\0';
      bufout[token_count++] = word;
    }
    if (token_count >= max_count) break;
  }
  bufout[token_count] = NULL;
  return token_count;
}


void interface() {
  tcgetattr(STDIN_FILENO, &orig_set);
  new_set = orig_set;
  new_set.c_lflag &= ~(ICANON | ECHO | ISIG);
  tcsetattr(STDIN_FILENO, TCSANOW, &new_set);


  char pwd[PATH_MAX];
  if (getcwd(pwd, sizeof(pwd)) == NULL) {
    perror("pwd error");
  }
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
        }
        prompt(pwd);
        continue;
      } else {
        fflush(stdout);
        // process(command);
        command_index = 0;
        memset(command, 0, sizeof(command));
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
  tcsetattr(STDIN_FILENO, TCSANOW, &orig_set);
}

int main() {
  //  interface();

  char input[] = "ls -la > lsout < echo \"finished\"";
  char* tokens[100];

  int count = command_lexer(input, tokens, 100);

  for (int i = 0; i < count; i++) {
    printf(": %s\n", tokens[i]);
  }

  for(int i = 0; i < count; i++) {
    free(tokens[i]);
  }
  
  return 0;
}
