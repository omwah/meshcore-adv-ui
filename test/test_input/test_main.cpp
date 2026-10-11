// Input filtering and the Cardputer keymap: the rules that keep navigation
// one-step-per-press.
#include <unity.h>
#include "ui/input/input.h"
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
  return UNITY_END();
}
