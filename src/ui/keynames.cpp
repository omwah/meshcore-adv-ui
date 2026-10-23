#include "ui/keynames.h"
#include <string.h>

namespace ui {

#ifdef MC_BOARD_TDECK
// T-Deck: Alt is Esc, Sym+Space is Tab, Sym+Mic is Menu, Sym+Shift h/j/k/l
// are the arrows.
static const char* const kNames[][2] = {
  {"opt", "sym+mic"},
  {"esc", "alt"},
  {"tab", "sym+spc"},
  {"fn", "sym+sh"},
  {"fn+,/", "sym+sh h/l"},
  {nullptr, nullptr},
};
const char* const kCursorHelp = "Sym+Shift h/l move cursor";
#else
static const char* const kNames[][2] = {{nullptr, nullptr}};
const char* const kCursorHelp = "Fn + , / move cursor";
#endif

const char* keyName(const char* key, bool vim) {
  if (vim && !strcmp(key, "^v")) return "j/k";
  if (vim && !strcmp(key, "<>")) return "h/l";
  for (const auto* n = kNames; (*n)[0]; n++)
    if (!strcmp((*n)[0], key)) return (*n)[1];
  return key;
}

}  // namespace ui
