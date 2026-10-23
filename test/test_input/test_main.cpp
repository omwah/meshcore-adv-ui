// Input filtering and the Cardputer keymap: the rules that keep navigation
// one-step-per-press.
#include <unity.h>
#include "ui/input/input.h"
#include "ui/input/vim.h"
#include "ui/widgets.h"
extern "C" {
#include "cardputer_keymap.h"
}

using namespace ui;

static hal::KeyEvent down(uint8_t u) { return {u, 0, true}; }
static hal::KeyEvent up(uint8_t u) { return {u, 0, false}; }
static const uint8_t ENTER = 0x28, ESC = 0x29, UP = 0x52, A = 0x04;

static int drain(Input& in, Key* out = nullptr, int max = 0) {
  Key k;
  int n = 0;
  while (in.next(k)) {
    if (out && n < max) out[n] = k;
    n++;
  }
  return n;
}

void test_enter_fires_once_per_press_even_when_held() {
  Input in;
  in.feed(down(ENTER), 0);
  for (uint32_t t = 0; t < 3000; t += 10) in.tick(t);
  in.feed(up(ENTER), 3000);
  Key k[4];
  TEST_ASSERT_EQUAL(1, drain(in, k, 4));
  TEST_ASSERT_EQUAL(K_ENTER, k[0].code);
}

void test_chatter_within_lockout_is_ignored() {
  Input in;
  in.setLockout(60);
  in.feed(down(ENTER), 0);
  in.feed(up(ENTER), 15);
  in.feed(down(ENTER), 30);   // bounce
  in.feed(up(ENTER), 40);
  in.feed(down(ENTER), 200);  // a real second press
  TEST_ASSERT_EQUAL(2, drain(in));
}

void test_arrows_repeat_after_delay() {
  Input in;
  in.feed(down(UP), 0);
  for (uint32_t t = 0; t <= Input::REPEAT_DELAY + 3 * Input::REPEAT_EVERY; t += 5) in.tick(t);
  Key k[8];
  int n = drain(in, k, 8);
  TEST_ASSERT_EQUAL(1 + 4, n);   // press, then repeats at delay, +80, +160, +240
  TEST_ASSERT_FALSE(k[0].repeat);
  TEST_ASSERT_TRUE(k[1].repeat);
}

void test_letters_do_not_repeat() {
  Input in;
  in.feed(down(A), 0);
  for (uint32_t t = 0; t < 2000; t += 10) in.tick(t);
  TEST_ASSERT_EQUAL(1, drain(in));
}

void test_esc_on_release_and_long_esc() {
  Input in;
  in.feed(down(ESC), 0);
  in.tick(100);
  TEST_ASSERT_EQUAL(0, drain(in));  // nothing until release
  in.feed(up(ESC), 150);
  Key k[2];
  TEST_ASSERT_EQUAL(1, drain(in, k, 2));
  TEST_ASSERT_EQUAL(K_ESC, k[0].code);

  in.feed(down(ESC), 1000);
  for (uint32_t t = 1000; t < 2000; t += 10) in.tick(t);
  in.feed(up(ESC), 2000);
  TEST_ASSERT_EQUAL(1, drain(in, k, 2));
  TEST_ASSERT_EQUAL(K_LONG_ESC, k[0].code);   // and no short Esc after it
}

void test_flush_swallows_held_key() {
  Input in;
  in.feed(down(UP), 0);
  drain(in);
  in.flush();   // screen changed while Up is held
  for (uint32_t t = 0; t < 2000; t += 10) in.tick(t);
  TEST_ASSERT_EQUAL(0, drain(in));
  in.feed(up(UP), 2000);
  in.feed(down(UP), 2200);
  TEST_ASSERT_EQUAL(1, drain(in));   // next real press works
}

void test_list_stops_at_ends() {
  ListNav nav;
  Key down{K_DOWN, 0, false}, upk{K_UP, 0, false};
  for (int i = 0; i < 10; i++) nav.key(down, 4, 3);
  TEST_ASSERT_EQUAL(3, nav.sel);
  for (int i = 0; i < 10; i++) nav.key(upk, 4, 3);
  TEST_ASSERT_EQUAL(0, nav.sel);
}

// Keymap: ";" (row 2, col 11) is Up in navigation, ';' in text mode.
void test_keymap_text_mode_swaps_arrow_keys() {
  cardputer_keymap_t st = {};
  cardputer_keymap_reset(&st);
  kbd_report_t r;
  TEST_ASSERT_TRUE(cardputer_keymap_event(&st, 2, 11, true, &r));
  TEST_ASSERT_EQUAL_HEX8(0x52, r.key[0]);
  cardputer_keymap_event(&st, 2, 11, false, &r);

  cardputer_keymap_text_mode(&st, true);
  cardputer_keymap_event(&st, 2, 11, true, &r);
  TEST_ASSERT_EQUAL_HEX8(0x33, r.key[0]);   // ';'
  cardputer_keymap_event(&st, 2, 11, false, &r);

  // Fn + ; gives Up again while typing.
  cardputer_keymap_event(&st, 2, 0, true, &r);
  cardputer_keymap_event(&st, 2, 11, true, &r);
  TEST_ASSERT_EQUAL_HEX8(0x52, r.key[0]);
}

void test_keymap_esc_stays_esc_in_text_mode() {
  cardputer_keymap_t st = {};
  cardputer_keymap_text_mode(&st, true);
  cardputer_keymap_reset(&st);
  kbd_report_t r;
  cardputer_keymap_event(&st, 0, 0, true, &r);
  TEST_ASSERT_EQUAL_HEX8(0x29, r.key[0]);
}

void test_keymap_opt_is_menu() {
  cardputer_keymap_t st = {};
  cardputer_keymap_reset(&st);
  kbd_report_t r;
  TEST_ASSERT_TRUE(cardputer_keymap_event(&st, 3, 1, true, &r));
  TEST_ASSERT_EQUAL_HEX8(0x65, r.key[0]);
  TEST_ASSERT_EQUAL(K_MENU, Input::translate(0x65, 0));
}

// ---------------------------------------------------------------- vim keys

static Key ch(char c, uint8_t mods = 0) { return Key{(uint16_t)c, mods, false, 0}; }

// Feed a string of typed keys; returns the commands that came out.
static int vimType(Vim& v, const char* s, Key* out, int max, uint32_t t = 0) {
  int n = 0;
  for (; *s; s++, t += 100) {
    Key k;
    if (v.feed(ch(*s), t, k) && n < max) out[n++] = k;
  }
  return n;
}

void test_vim_motions() {
  Vim v;
  Key o[4];
  TEST_ASSERT_EQUAL(4, vimType(v, "jkhl", o, 4));
  TEST_ASSERT_EQUAL(K_DOWN, o[0].code);
  TEST_ASSERT_EQUAL(K_UP, o[1].code);
  TEST_ASSERT_EQUAL(K_LEFT, o[2].code);
  TEST_ASSERT_EQUAL(K_RIGHT, o[3].code);
  TEST_ASSERT_EQUAL(0, o[0].count);
}

void test_vim_counts() {
  Vim v;
  Key o[2];
  TEST_ASSERT_EQUAL(1, vimType(v, "12j", o, 2));
  TEST_ASSERT_EQUAL(K_DOWN, o[0].code);
  TEST_ASSERT_EQUAL(12, o[0].count);
  TEST_ASSERT_EQUAL(1, vimType(v, "3G", o, 2));
  TEST_ASSERT_EQUAL(K_GOTO, o[0].code);
  TEST_ASSERT_EQUAL(3, o[0].count);
  // 0 alone is not a count.
  TEST_ASSERT_EQUAL(1, vimType(v, "0", o, 2));
  TEST_ASSERT_EQUAL('0', o[0].code);
}

void test_vim_g_commands() {
  Vim v;
  Key o[4];
  TEST_ASSERT_EQUAL(4, vimType(v, "ggGgtgT", o, 4));
  TEST_ASSERT_EQUAL(K_HOME, o[0].code);
  TEST_ASSERT_EQUAL(K_END, o[1].code);
  TEST_ASSERT_EQUAL(K_TAB_NEXT, o[2].code);
  TEST_ASSERT_EQUAL(K_TAB_PREV, o[3].code);
  TEST_ASSERT_EQUAL(1, vimType(v, "3gt", o, 4));
  TEST_ASSERT_EQUAL(K_TAB_N, o[0].code);
  TEST_ASSERT_EQUAL(3, o[0].count);
  TEST_ASSERT_EQUAL(2, vimType(v, "ZZZQ", o, 4));
  TEST_ASSERT_EQUAL(K_SEND, o[0].code);
  TEST_ASSERT_EQUAL(K_DISCARD, o[1].code);
  TEST_ASSERT_EQUAL(2, vimType(v, "]u[u", o, 4));
  TEST_ASSERT_EQUAL(K_NEXT_UNREAD, o[0].code);
  TEST_ASSERT_EQUAL(K_PREV_UNREAD, o[1].code);
  // Not a command: both keys dropped.
  TEST_ASSERT_EQUAL(0, vimType(v, "gx", o, 4));
}

void test_vim_pending_and_timeout() {
  Vim v;
  Key o;
  TEST_ASSERT_FALSE(v.feed(ch('2'), 0, o));
  TEST_ASSERT_FALSE(v.feed(ch('g'), 100, o));
  TEST_ASSERT_EQUAL_STRING("2g", v.pending());
  // Too slow: the half-typed command is dropped, and j is a plain motion.
  TEST_ASSERT_TRUE(v.feed(ch('j'), 100 + Vim::PENDING_MS, o));
  TEST_ASSERT_EQUAL(K_DOWN, o.code);
  TEST_ASSERT_EQUAL(0, o.count);
  TEST_ASSERT_EQUAL_STRING("", v.pending());
  // Esc cancels a pending command and does nothing else.
  TEST_ASSERT_FALSE(v.feed(ch('g'), 0, o));
  TEST_ASSERT_FALSE(v.feed(Key{K_ESC, 0, false, 0}, 10, o));
  TEST_ASSERT_TRUE(v.feed(Key{K_ESC, 0, false, 0}, 20, o));
  TEST_ASSERT_EQUAL(K_ESC, o.code);
}

void test_vim_ctrl_and_others() {
  Vim v;
  Key o;
  TEST_ASSERT_TRUE(v.feed(ch('d', KM_CTRL), 0, o));
  TEST_ASSERT_EQUAL(K_HALF_DN, o.code);
  TEST_ASSERT_TRUE(v.feed(ch('u', KM_CTRL), 0, o));
  TEST_ASSERT_EQUAL(K_HALF_UP, o.code);
  TEST_ASSERT_TRUE(v.feed(ch('f', KM_CTRL), 0, o));
  TEST_ASSERT_EQUAL(K_PGDN, o.code);
  TEST_ASSERT_TRUE(v.feed(ch('o', KM_CTRL), 0, o));
  TEST_ASSERT_EQUAL(K_JUMP_BACK, o.code);
  TEST_ASSERT_TRUE(v.feed(ch('q'), 0, o));
  TEST_ASSERT_EQUAL(K_ESC, o.code);
  TEST_ASSERT_TRUE(v.feed(ch('i'), 0, o));
  TEST_ASSERT_EQUAL(K_COMPOSE, o.code);
  // Keys that aren't commands pass through (y/n in a confirm box).
  TEST_ASSERT_TRUE(v.feed(ch('y'), 0, o));
  TEST_ASSERT_EQUAL('y', o.code);
  TEST_ASSERT_TRUE(v.feed(Key{K_ENTER, 0, false, 0}, 0, o));
  TEST_ASSERT_EQUAL(K_ENTER, o.code);
}

void test_vim_letters_repeat_only_when_on() {
  static const uint8_t J = 0x0d;
  Input in;
  in.feed(down(J), 0);
  for (uint32_t t = 0; t < 1000; t += 10) in.tick(t);
  TEST_ASSERT_EQUAL(1, drain(in));
  in.feed(up(J), 1000);
  in.setVimRepeat(true);
  in.feed(down(J), 2000);
  for (uint32_t t = 2000; t < 3000; t += 10) in.tick(t);
  TEST_ASSERT_TRUE(drain(in) > 5);
}

void test_list_half_page_and_goto() {
  ListNav n;
  Key k{K_HALF_DN, 0, false, 0};
  TEST_ASSERT_TRUE(n.key(k, 20, 6));
  TEST_ASSERT_EQUAL(3, n.sel);
  k.code = K_GOTO;
  k.count = 15;
  TEST_ASSERT_TRUE(n.key(k, 20, 6));
  TEST_ASSERT_EQUAL(14, n.sel);
  k.count = 99;
  TEST_ASSERT_TRUE(n.key(k, 20, 6));
  TEST_ASSERT_EQUAL(19, n.sel);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_enter_fires_once_per_press_even_when_held);
  RUN_TEST(test_chatter_within_lockout_is_ignored);
  RUN_TEST(test_arrows_repeat_after_delay);
  RUN_TEST(test_letters_do_not_repeat);
  RUN_TEST(test_esc_on_release_and_long_esc);
  RUN_TEST(test_flush_swallows_held_key);
  RUN_TEST(test_list_stops_at_ends);
  RUN_TEST(test_keymap_text_mode_swaps_arrow_keys);
  RUN_TEST(test_keymap_esc_stays_esc_in_text_mode);
  RUN_TEST(test_keymap_opt_is_menu);
  RUN_TEST(test_vim_motions);
  RUN_TEST(test_vim_counts);
  RUN_TEST(test_vim_g_commands);
  RUN_TEST(test_vim_pending_and_timeout);
  RUN_TEST(test_vim_ctrl_and_others);
  RUN_TEST(test_vim_letters_repeat_only_when_on);
  RUN_TEST(test_list_half_page_and_goto);
  return UNITY_END();
}
