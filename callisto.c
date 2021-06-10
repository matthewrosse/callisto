#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct termios orig_termios;

void disable_raw_mode();
void enable_raw_mode();

int main() {
  enable_raw_mode();

  char c;
  while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
    // iscntrl(char) test whether a character is a control character. (ASCII 0-31 and 127) (<ctype.h>)
    if (iscntrl(c)) {
      printf("%d\n", c);
    }
    else {
      printf("%d ('%c')\n", c, c);
    }
  }
  return 0;
}

void disable_raw_mode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode() {
  tcgetattr(STDIN_FILENO, &orig_termios);
/* we use it to call disable_raw_mode() automatically when program exits,
 * either by returning from main(), or by calling the exit() function */
  atexit(disable_raw_mode);

  struct termios raw = orig_termios;

  tcgetattr(STDIN_FILENO, &raw);
  /* The ECHO feature causes each key you type to be printed to the terminal */
  /* ECHO is a bitflag. We use the bitwise-NOT operator (~) to get opposite values,
   * then bitwise-AND this value with the flags field, which forces the fourth bit in the flags field
   * to become 0, and causes every other bit to retain its current value. */
   // Similar behaviour when you type your password after "sudo" command
   //
   // ICANON flag allows us to turn off canonical mode. (reading input byte-by-byte instead of line-by-line)
  raw.c_iflag &= ~(IXON | ICRNL);
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
  /* The TCSAFLUSH argument specifies when to apply the change, in this case it waits
   * for all pending output to be written to the terminal, and also discards any input
   * that hasn't been read */
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}
