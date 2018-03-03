#include <cctype>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <list>

#include <curses.h>

#define KEY_CTRL(ch) (ch ^ 0x40)

namespace {

typedef std::list<std::string> Lines;

int
getHeight()
{
  int height, width;
  getmaxyx(stdscr, height, width);
  return height - 2;
}

int
getWidth()
{
  int height, width;
  getmaxyx(stdscr, height, width);
  return width;
}

void
scrollUp()
{
  scrollok(stdscr, true);
  scrl(1);
  scrollok(stdscr, false);
}

void
scrollDown()
{
  scrollok(stdscr, true);
  scrl(-1);
  scrollok(stdscr, false);
}

void
showlines(Lines::iterator start, Lines::iterator end)
{
  int height, width;
  auto it = start;
  getmaxyx(stdscr, height, width);
  erase();
  for (int i = 0; i < (height - 2) && it != end; ++i, ++it) {
    mvwprintw(stdscr, i, 0, "%-*s", width, it->c_str());
  }
}

void
cleanup()
{
  delwin(stdscr);
  endwin();
}

} // namespace

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
  int yCursor = 0;
  int xCursor = 0;

  Lines lines;
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

  initscr();
  atexit(cleanup);
  keypad(stdscr, true);
  noecho();

  auto it = lines.begin();
  auto cursorIt = it;

  bool redraw = true;
  bool redrawLine = false;
  bool commandMode = false;
  size_t commandXCursor = 0;
  std::string command;

  do {
    std::string notification;

    if (commandMode) {
      if (std::isprint(ch)) {
        command.insert(commandXCursor++, 1, ch);
        mvprintw(getHeight() + 1, 0, "%-*s", getWidth(), command.c_str());
        move(getHeight() + 1, commandXCursor);
	refresh();
	continue;
      } else if (ch == KEY_CTRL('J')) {
        notification = "Command failed!";
      }
      ch = 0;
      commandMode = false;
      command.erase();
    }

    if (ch == KEY_CTRL('I')) {
      commandMode = true;
      commandXCursor = 0;
      mvprintw(getHeight() + 1, 0, "%-*s", getWidth(), "");
      move(getHeight() + 1, 0);
      refresh();
      continue;
    }

    if (ch == KEY_RIGHT) {
      if (xCursor != cursorIt->size()) {
        ++xCursor;
      } else {
        auto next = cursorIt;
        if (++next == lines.end())
          continue;
        xCursor = 0;
        cursorIt = next;
        ++yCursor;
      }
    }
    if (ch == KEY_LEFT) {
      if (xCursor != 0) {
        --xCursor;
      } else {
        if (cursorIt == lines.begin())
          continue;
        --cursorIt;
        xCursor = cursorIt->size();
        --yCursor;
      }
    }
    if (ch == KEY_DOWN) {
      auto next = cursorIt;
      if (++next == lines.end())
        continue;
      cursorIt = next;
      ++yCursor;
    }
    if (ch == KEY_UP) {
      if (cursorIt == lines.begin())
        continue;
      --cursorIt;
      --yCursor;
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
        redrawLine = true;
      } else if (cursorIt != lines.begin()){
        auto prev = cursorIt--;
        xCursor = cursorIt->size();
        (*cursorIt) += *prev;
        lines.erase(prev);
        if (yCursor == 0) {
          --yFrame;
          it = cursorIt;
          redrawLine = true;
        } else {
          --yCursor;
          redraw = true;
        }
      }
    }
    if (ch == KEY_CTRL('K')) {
      if (cursorIt->size() != xCursor) {
        cursorIt->erase(xCursor);
      } else {
        auto next = cursorIt;
        if (++next != lines.end()) {
          (*cursorIt) += *next;
          lines.erase(next);
        }
      }
      redraw = true;
    }
    if (ch == KEY_CTRL('J')) {
      auto prev = cursorIt++;
      size_t pos = prev->find_first_not_of(" \t");
      cursorIt = lines.emplace(cursorIt,
                               prev->substr(0, pos) + prev->substr(xCursor));
      prev->erase(xCursor);
      xCursor = pos;
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
      auto last = --lines.end();
      for (auto line = lines.begin(); line != last; ++line) {
        tmpfile << *line << std::endl;
      }
      if (last->size() > 0) {
        tmpfile << *last;
      }
      tmpfile.close();
      rename(tmpname.c_str(), argv[1]);
      notification = "Saved.";
    }
    if (std::isprint(ch)) {
      cursorIt->insert(xCursor++, 1, ch);
      redrawLine = true;
    }
    if (xCursor > cursorIt->size()) {
      xCursor = cursorIt->size();
    }
    if (yCursor == getHeight()) {
      scrollUp();
      --yCursor;
      ++yFrame;
      ++it;
      redrawLine = true;
    } else if (yCursor == -1) {
      scrollDown();
      ++yCursor;
      --yFrame;
      --it;
      redrawLine = true;
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
    mvprintw(getHeight() + 1, 0, "%-*s", getWidth(), notification.c_str());
    move(yCursor, xCursor);
    refresh();
  } while ((ch = getch()) != KEY_CTRL('C'));

  return 0;
}
