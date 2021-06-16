/* Includes */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/* Defines */
#define CALLISTO_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {
ARROW_LEFT = 1000,
ARROW_RIGHT,
ARROW_UP,
ARROW_DOWN,
PAGE_UP,
PAGE_DOWN
};

/* Data */

struct editor_config {
  int cx, cy;
  int screenrows;
  int screencols;
  struct termios orig_termios;
};

struct editor_config E;

/* Append buffer*/

struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0};
/* Declarations */

void die(const char*);
void disable_raw_mode();
void enable_raw_mode();
int editor_read_key();
void editor_process_keypress();
void editor_refresh_screen();
void editor_draw_rows(struct abuf *);
int get_window_size();
int get_cursor_position(int *, int *);
void init_editor();
void ab_append(struct abuf*, const char*, int);
void ab_free(struct abuf *);
void editor_move_cursor(int);

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
  E.cx = 0;
  E.cy = 0;
  if (get_window_size(&E.screenrows, &E.screencols) == -1)
    die("get_window_size");
}

/* Output */

void editor_draw_rows(struct abuf *ab) {
  for (int i = 0; i < E.screenrows; i++) {
    if (i == E.screenrows / 3) {
      char welcome[80];
      int welcomelen = snprintf(welcome, sizeof(welcome), "Callisto text editor -- version %s", CALLISTO_VERSION);
      if (welcomelen > E.screencols) {
        welcomelen = E.screencols;
      }
      int padding = (E.screencols - welcomelen) / 2;
      if (padding) {
        ab_append(ab, "~", 1);
        padding--;
      }
      while (padding--) {
        ab_append(ab, " ", 1);
      }
      ab_append(ab, welcome, welcomelen);
    }
    else {
      ab_append(ab, "~", 1);
    }

    ab_append(ab, "\x1b[K", 3);
    if (i < E.screenrows - 1) {
      ab_append(ab, "\r\n", 2);
    }
  }
}

void editor_refresh_screen() {
  struct abuf ab = ABUF_INIT;

  ab_append(&ab, "\x1b[?25l", 6);
  ab_append(&ab, "\x1b[H", 3);

  editor_draw_rows(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
  ab_append(&ab, buf, strlen(buf));

  ab_append(&ab, "\x1b[?25h", 6);

  write(STDOUT_FILENO, ab.b, ab.len);
  ab_free(&ab);
}

/* Input */

void editor_move_cursor(int key) {
  switch (key) {
    case ARROW_LEFT:
      if (E.cx != 0) {
        E.cx--;
      }
      break;
    case ARROW_RIGHT:
      if (E.cx != E.screencols - 1) {
        E.cx++;
      }
      break;
    case ARROW_UP:
      if (E.cy != 0) {
        E.cy--;
      }
      break;
    case ARROW_DOWN:
      if (E.cy != E.screenrows - 1) {
        E.cy++;
      }
      break;
  }
}

void editor_process_keypress() {
  int c = editor_read_key();

  switch(c) {
    case CTRL_KEY('q'):
      write(STDIN_FILENO, "\x1b[2J", 4);
      write(STDIN_FILENO, "\x1b[H", 3);
      exit(0);
      break;

    case PAGE_UP:
    case PAGE_DOWN:
    {
      int times = E.screenrows;
      while (times--) {
        editor_move_cursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
    }
    break;

    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editor_move_cursor(c);
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

int editor_read_key() {
  int nread;
  char c;
  while((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) {
      die("read");
    }
  }

  if (c == '\x1b') {
    char seq[3];

    if (read(STDIN_FILENO, &seq[0], 1) != 1)
      return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1)
      return '\x1b';

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1)
          return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
            case '5':
              return PAGE_UP;
            case '6':
              return PAGE_DOWN;
          }
        }
      } else {

        switch (seq[1]) {
          case 'A': return ARROW_UP;
          case 'B': return ARROW_DOWN;
          case 'C': return ARROW_RIGHT;
          case 'D': return ARROW_LEFT;
        }
      }
    }

    return '\x1b';
  }
  else {
    return c;
  }
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

/* Append buffer */

void ab_append(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);

  if (new == NULL)
    return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void ab_free(struct abuf *ab) {
  free(ab->b);
}
