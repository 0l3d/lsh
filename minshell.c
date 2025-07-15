#include <limits.h>
#include <pty.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>

// gcc minshell.c -I/usr/include/lua5.4 -L/usr/lib/ -Wl,-Bstatic -llua5.4 -Wl,-Bdynamic -lc -lm -o minshell

#define MAX_TOKENS 200
#define MAX_PIPES 100

struct termios orig_set, new_set;

void prompt(char *path) {
  printf("%s -> ", path);
  fflush(stdout);
}

void process(char* command[]) {
  pid_t pid;

  pid = fork();
  if (pid == -1) {
    fprintf(stderr, "forkpty is failed");
    return;
  }
  

  if (pid == 0) {
    if (execvp(command[0], command) == -1) {
      perror("Command execution failed: ");
      return;
    } else {
      printf("command is not executed");
    }
  } else {
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
	char* tokens[MAX_TOKENS];

	// PIPE HANDLING
	int special_proc = 0;
	int its_pipe = 0;
	int c_pipe = 0;
	int c_pipe_pos[MAX_PIPES];
	// PIPE HANDLING
	
	int tokenc = command_lexer(command, tokens, MAX_TOKENS);
	for (int i = 0; i < tokenc; i++) {
	  if (strcmp(tokens[i], "<") == 0) {
	    special_proc = 1;
	    pid_t pid = fork();
	    if (pid == -1) {
	      perror("fork is failed");
	      break;
	    }
	    if (pid == 0) {
	      int fd = open(tokens[i + 1], O_RDONLY);
	      if (fd == -1) {
		perror("file open failed");
		break;
	      }
	      dup2(fd, 0);
	      close(fd);
	      tokens[i] = NULL;
	      execvp(tokens[0], tokens);
	    } else {
	      waitpid(pid, NULL, 0);
	    }
	  } else if (strcmp(tokens[i], ">") == 0) {
	    special_proc = 1;
	    pid_t pid = fork();
	    if (pid == -1) {
	      perror("fork is failed");
	      break;
	    }

	    if (pid == 0) {
	      int fd = open(tokens[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
	      if (fd == -1) {
		perror("file open failed");
		break;
	      }

	      dup2(fd, STDOUT_FILENO);
	      close(fd);
	      tokens[i] = NULL;
	      execvp(tokens[0], tokens);
	    }
	  } else if (strcmp(tokens[i], "|") == 0) {
	    special_proc = 1;
	    c_pipe_pos[c_pipe++] = i;
	    its_pipe = 1;
	    
	  }
	}

	if (its_pipe) {
	  int start = 0;
	  int prev_fd;
	  for (int i = 0; i <= c_pipe; i++) {
	    int end = (i < c_pipe) ? c_pipe_pos[i] : tokenc;
	    int fd[2];
	    char* pipetok[MAX_TOKENS];
	    if (pipe(fd) < 0) exit(1);
	    
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
	      execvp(pipetok[0], pipetok);
	      exit(0);
	    } else  {
	      close(fd[1]);
	      if (start != 0) {
		close(prev_fd);
	      }
	      prev_fd = fd[0];
	      waitpid(pid, NULL, 0);
	    }
	    start = end + 1;
	  }
	}
	
	if (!special_proc && !its_pipe) {
	  process(tokens);
	}
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
  interface();
  return 0;
}
