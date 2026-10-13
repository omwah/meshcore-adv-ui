// Entry points to every screen. Screens are file-local singletons; other
// screens open them through these functions.
#pragma once
#include "ui/app.h"

#ifndef MC_UI_REMOTE
#define MC_UI_REMOTE 0   // remote node mode (set by builds with Bluetooth)
#endif

namespace ui { namespace screens {

Screen* home();
Screen* boot();
void openThread(App& a, model::ThreadId t);
// The open chat's messages from the "New" line down (-1: no line). For tests.
int chatNewCount(App& a);
void openContact(App& a, int contactIdx);
void openChannel(App& a, int channelIdx);
void openAdmin(App& a, int contactIdx);
void openMainMenu(App& a);
void openSettings(App& a);
void openRadio(App& a);
void openDiag(App& a);
void openPacketLog(App& a);
void openGps(App& a);
void openHeard(App& a, bool repeatersOnly);
void openBackup(App& a);
void openJoinChannel(App& a);
#if MC_UI_REMOTE
void openRemote(App& a);   // remote mode: the link to the node
#endif
// Scrollable text page (results, keys, help).
void openText(App& a, const char* title, const char* body);

}}  // namespace ui::screens
