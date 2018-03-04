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

Lines::iterator
find(std::string str,  Lines::iterator start, Lines::iterator end,
     int &x, int &y)
{
  int nx = x + 1;
  int ny = y;
  for (auto line = start; line != end; ++line, ++ny) {
    if ((nx = line->find(str, nx)) != std::string::npos) {
      x = nx;
      y = ny;
      return line;
    }
    nx = 0;
  }
  return end;
}

Lines::reverse_iterator
rfind(std::string str,  Lines::reverse_iterator start,
      Lines::reverse_iterator end, int &x, int &y)
{
  std::string::size_type nx;
  int ny = y;
  auto line = start;
  if (x == 0) {
    nx = std::string::npos;
    ++line;
    --ny;
  } else {
    nx = x - 1;
  }
  for ( ; line != end; ++line, --ny) {
    if ((nx = line->rfind(str, nx)) != std::string::npos) {
      x = nx;
      y = ny;
      return line;
    }
    nx = std::string::npos;
  }
  return end;
}

std::string
getCommand()
{
  static std::string command;
  static size_t cursor = 0;

  while (true) {
    mvprintw(getHeight() + 1, 0, "%-*s", getWidth(), command.c_str());
    move(getHeight() + 1, cursor);
    refresh();

    int ch = getch();

    if (std::isprint(ch)) {
      command.insert(cursor++, 1, ch);
    } else if (ch == KEY_CTRL('A')) {
      cursor = 0;
    } else if (ch == KEY_CTRL('E')) {
      cursor = command.size();
    } else if (ch == KEY_CTRL('K')) {
      command.erase(cursor);
    } else if (ch == KEY_RIGHT) {
      if (cursor != command.size()) {
        ++cursor;
      }
    } else if (ch == KEY_LEFT) {
      if (cursor != 0) {
        --cursor;
      }
    } else if (ch == KEY_BACKSPACE || ch == KEY_CTRL('?')) {
      if (cursor != 0) {
        command.erase(--cursor, 1);
      }
    } else if (ch == KEY_CTRL('J')) {
      cursor = 0;
      return command;
    } else {
      return "";
    }
  }

  return command;
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

  do {
    std::string notification;

    if (ch == KEY_CTRL('I')) {
      std::string command = getCommand();
      if (command.size() == 0) {
        ;
      } else if (command[0] == '/') {
        auto result = find(command.substr(1), cursorIt, lines.end(),
                           xCursor, yCursor);
        if (result != lines.end()) {
          cursorIt = result;
          while (yCursor >= getHeight()) {
            --yCursor;
            ++yFrame;
            ++it;
          }
          redraw = true;
        } else {
          notification = "Not found!";
        }
      } else if (command[0] == '?') {
        auto result = rfind(command.substr(1),
                            --Lines::reverse_iterator(cursorIt),
                            lines.rend(), xCursor, yCursor);
        if (result != lines.rend()) {
          cursorIt = --result.base();
          while (yCursor < 0) {
            ++yCursor;
            --yFrame;
            --it;
          }
          redraw = true;
        } else {
          notification = "Not found!";
        }
      } else if (command[0] == ':') {
        int lineno = std::atoi(&(command.c_str()[1]));
        if (lineno < 1 || lineno > lines.size()) {
          notification = "Invalid line number!";
        } else {
          it = cursorIt = lines.begin();
          xCursor = yCursor = yFrame = 0;
          while (yFrame < lineno) {
            ++yFrame;
            ++it;
          }
          cursorIt = it;
          redraw = true;
        }
      } else if (command[0] == '^') {
        it = cursorIt = lines.begin();
        xCursor = yCursor = yFrame = 0;
        redraw = true;
      } else if (command[0] == '$') {
        cursorIt = lines.end();
        it = --cursorIt;
        xCursor = cursorIt->size();
        yCursor = 0;
        yFrame = lines.size() - 1;
        while (yCursor < getHeight() - 1 && it != lines.begin()) {
          ++yCursor;
          --yFrame;
          --it;
        }
        redraw = true;
      } else {
        notification = "Unknown command!";
      }
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
    if (ch == KEY_BACKSPACE || ch == KEY_CTRL('?')) {
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
