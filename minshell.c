#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

struct termios orig_set, new_set;

void prompt(char* path) {
  // DEFAULT
  printf("%s -> ", path);
  fflush(stdout);
}

void init_shell() {
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
  if (history == NULL) {
    fprintf(stderr, "Memory Allocation error.");
  }
  char command[256] = {0};
  unsigned int command_index = 0;
  char buffer[3] = {0};
  int history_i = 0;
  int history_cur = -1;
  while (1) {
    ssize_t n = read(STDIN_FILENO, buffer, 1);
    if (n <= 0) continue;
    if (buffer[0] == '\n' || buffer[0] == '\r') { // ENTER IMPLEMENTATION
      printf("\n");
      fflush(stdout);
      history[history_i++] = strdup(command);
      history_cur = history_i;
      if (strcmp(command, "exit") == 0) {
	break;
      } else if (strncmp(command, "cd ", 3) == 0) { // CD COMMAND
	char* path = command + 3;
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
	system(command);
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
  init_shell();
  return 0;
}
