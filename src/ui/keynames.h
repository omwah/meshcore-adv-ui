// Key names shown to the user (footer hints, prompts). Screens name keys by
// what they are on the Cardputer ("opt", "fn+,/"); the board's own legends
// replace them here.
#pragma once

namespace ui {

// The board's name for a hint key; anything without a replacement as is.
const char* keyName(const char* key);
// Prompt line telling how to move the cursor while typing.
extern const char* const kCursorHelp;

}  // namespace ui
