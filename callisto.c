/* Includes */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/* Defines */
#define CTRL_KEY(k) ((k) & 0x1f)

/* Data */

struct editor_config {
  int screenrows;
  int screencols;
  struct termios orig_termios;
};

struct editor_config E;

/* Declarations */

void die(const char*);
void disable_raw_mode();
void enable_raw_mode();
char editor_read_key();
void editor_process_keypress();
void editor_refresh_screen();
void editor_draw_rows();
int get_window_size();
int get_cursor_position(int *, int *);
void init_editor();

/* Init */

int main() {
  enable_raw_mode();
  init_editor();

  while (1) {
    editor_refresh_screen();
    editor_process_keypress();
  }
  return 0;
}

void init_editor() {
  if (get_window_size(&E.screenrows, &E.screencols) == -1)
    die("get_window_size");
}

/* Output */

void editor_draw_rows() {
  for (int i = 0; i < E.screenrows; i++) {
    write(STDIN_FILENO, "~\r\n", 3);
  }
}

void editor_refresh_screen() {
  write(STDIN_FILENO, "\x1b[2J", 4);
  write(STDIN_FILENO, "\x1b[H", 3);

  editor_draw_rows();
  write(STDIN_FILENO, "\x1b[H", 3);
}

/* Input */

void editor_process_keypress() {
  char c = editor_read_key();

  switch(c) {
    case CTRL_KEY('q'):
      write(STDIN_FILENO, "\x1b[2J", 4);
      write(STDIN_FILENO, "\x1b[H", 3);
      exit(0);
      break;
  }
}

/* Terminal */

int get_cursor_position(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
    return -1;

  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1)
      break;
    if (buf[i] == 'R')
      break;
    i++;
  }
  buf[i] = '\0';

  if (buf[0] != '\x1b' || buf[1] != '[')
    return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
    return -1;

  return 0;
}

int get_window_size(int *rows, int *cols) {
  struct winsize ws;

  if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
      return -1;
    return get_cursor_position(rows, cols);
  }
  else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

char editor_read_key() {
  int nread;
  char c;
  while((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) {
      die("read");
    }
  }
    return c;
}

void die(const char *s) {
  write(STDIN_FILENO, "\x1b[2J", 4);
  write(STDIN_FILENO, "\x1b[H", 3);
  perror(s);
  exit(1);
}

void disable_raw_mode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
    die("tcsetattr");
  }
}

void enable_raw_mode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {
    die("tcgetattr");
  }
/* we use it to call disable_raw_mode() automatically when program exits,
 * either by returning from main(), or by calling the exit() function */
  atexit(disable_raw_mode);

  struct termios raw = E.orig_termios;

  tcgetattr(STDIN_FILENO, &raw);
  /* The ECHO feature causes each key you type to be printed to the terminal */
  /* ECHO is a bitflag. We use the bitwise-NOT operator (~) to get opposite values,
   * then bitwise-AND this value with the flags field, which forces the fourth bit in the flags field
   * to become 0, and causes every other bit to retain its current value. */
   // Similar behaviour when you type your password after "sudo" command
   //
   // ICANON flag allows us to turn off canonical mode. (reading input byte-by-byte instead of line-by-line)
  raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag &= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;
  /* The TCSAFLUSH argument specifies when to apply the change, in this case it waits
   * for all pending output to be written to the terminal, and also discards any input
   * that hasn't been read */
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1 && errno != EAGAIN) {
    die("read");
  }
}
