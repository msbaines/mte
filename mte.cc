#include <cctype>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <list>

#include <curses.h>

#define KEY_CTRL(ch) (ch ^ 0x40)

typedef std::list<std::string> LinesType;

static int
getHeight()
{
  int height, width;
  getmaxyx(stdscr, height, width);
  return height - 2;
}

static int
getWidth()
{
  int height, width;
  getmaxyx(stdscr, height, width);
  return width;
}

static void
scrollUp()
{
  scrollok(stdscr, true);
  scrl(1);
  scrollok(stdscr, false);
}

static void
scrollDown()
{
  scrollok(stdscr, true);
  scrl(-1);
  scrollok(stdscr, false);
}

static void
showlines(LinesType::iterator start, LinesType::iterator end)
{
  int height, width;
  LinesType::iterator it = start;
  getmaxyx(stdscr, height, width);
  erase();
  for (int i = 0; i < (height - 2) && it != end; ++i, ++it) {
    mvwprintw(stdscr, i, 0, "%-*s", width, it->c_str());
  }
}

static void
cleanup()
{
  delwin(stdscr);
  endwin();
}

int
main(int argc, char *argv[])
{
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
    exit(EXIT_FAILURE);
  }

  std::ifstream fin(argv[1]);
  if (!fin.is_open()) {
    perror("Can't open input file");
    exit(EXIT_FAILURE);
  }

  int ch = 0;
  size_t yFrame = 0;
  size_t yCursor = 0;
  size_t xCursor = 0;

  LinesType lines;
  std::string line;
  bool addNL = true;
  while (getline(fin, line)) {
    lines.emplace_back(std::move(line));
    if (fin.eof()) {
      addNL = false;
    }
  }
  if (addNL) {
    lines.emplace_back();
  }

  FILE *in = fopen("/dev/tty", "r");
  if (in == NULL) {
    perror("fopen(/dev/tty)");
    exit(EXIT_FAILURE);
  }

  newterm(NULL, stdout, in);
  atexit(cleanup);
  keypad(stdscr, true);
  noecho();

  LinesType::iterator it = lines.begin();
  LinesType::iterator cursorIt = it;

  bool redraw = true;
  bool redrawLine = false;

  do {
    std::string command;
    size_t prevXCursor = xCursor;
    if (ch == KEY_RIGHT && xCursor != cursorIt->size()) {
      ++xCursor;
    }
    if (ch == KEY_LEFT && xCursor != 0) {
      --xCursor;
    }
    if ((ch == KEY_DOWN) ||
        (ch == KEY_RIGHT && prevXCursor == cursorIt->size())) {
      if (yFrame + yCursor == lines.size() - 1) {
        continue;
      }
      if (yCursor == getHeight() - 1) {
        redrawLine = true;
        ++yFrame;
        ++it;
        scrollUp();
      } else {
        ++yCursor;
      }
      ++cursorIt;
      if (ch == KEY_RIGHT) {
        xCursor = 0;
      }
    }
    if ((ch == KEY_UP) ||
        (ch == KEY_LEFT && prevXCursor == 0)) {
      if (yFrame + yCursor == 0)
        continue;
      if (yCursor == 0) {
        redrawLine = true;
        --yFrame;
        --it;
        scrollDown();
      } else {
        --yCursor;
      }
      --cursorIt;
      if (ch == KEY_LEFT) {
        xCursor = cursorIt->size();
      }
    }
    if (ch == KEY_CTRL('A')) {
      xCursor = 0;
    }
    if (ch == KEY_CTRL('E')) {
      xCursor = getWidth();
    }
    if (ch == KEY_BACKSPACE ||
        ch == KEY_CTRL('?')) {
      if (xCursor > 0) {
        cursorIt->erase(--xCursor, 1);
        redraw = true;
      } else if (yCursor > 0){
        LinesType::iterator prev = cursorIt--;
        xCursor = cursorIt->size();
        (*cursorIt) += *prev;
        lines.erase(prev);
        --yCursor;
        redraw = true;
      }
    }
    if (ch == KEY_CTRL('K')) {
      if (cursorIt->size() != xCursor) {
        cursorIt->erase(xCursor);
      } else {
        LinesType::iterator next = cursorIt;
        if (++next != lines.end()) {
          (*cursorIt) += *next;
          lines.erase(next);
        }
      }
      redraw = true;
    }
    if (ch == KEY_CTRL('J')) {
      LinesType::iterator prev = cursorIt++;
      cursorIt = lines.emplace(cursorIt, prev->substr(xCursor));
      prev->erase(xCursor);
      xCursor = 0;
      if (yCursor < getHeight() - 1) {
        ++yCursor;
      } else {
        ++it;
        ++yFrame;
      }
      redraw = true;
    }
    if (ch == KEY_CTRL('X')) {
      std::string tmpname{argv[1]};
      tmpname += "~";
      std::ofstream tmpfile(tmpname);
      LinesType::iterator last = --lines.end();
      for (LinesType::iterator line = lines.begin(); line != last; ++line) {
        tmpfile << *line << std::endl;
      }
      if (last->size() > 0) {
        tmpfile << *last;
      }
      tmpfile.close();
      rename(tmpname.c_str(), argv[1]);
      command = "Saved.";
    }
    if (std::isprint(ch)) {
      cursorIt->insert(xCursor++, 1, ch);
      redrawLine = true;
    }
    if (xCursor > cursorIt->size()) {
      xCursor = cursorIt->size();
    }
    if (redrawLine) {
      mvprintw(yCursor, 0, "%-*s", getWidth(), cursorIt->c_str());
    } else if (redraw) {
      showlines(it, lines.end());
    }
    redraw = redrawLine = false;
    std::string status{argv[1]};
    status += ":" + std::to_string(yFrame + yCursor + 1) + ":"
                  + std::to_string(xCursor + 1) + ":";
    attron(A_REVERSE);
    mvprintw(getHeight(), 0, "%*s", getWidth(), status.c_str());
    attroff(A_REVERSE);
    mvprintw(getHeight() + 1, 0, "%-*s", getWidth(), command.c_str());
    move(yCursor, xCursor);
    refresh();
  } while ((ch = getch()) != KEY_CTRL('C'));

  return 0;
}
