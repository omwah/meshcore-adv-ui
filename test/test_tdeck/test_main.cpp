// The T-Deck keymap and trackball, as the UI sees them: the keys it has no
// legend for (Esc, Tab, Menu, Ctrl) and the arrows.
#include <unity.h>
#include "ui/input/input.h"
extern "C" {
#include "tdeck_keymap.h"
}

using namespace ui;

// Matrix places (column, row), as in tdeck_keymap.c.
static const int SYM[2] = {0, 2}, ALT[2] = {0, 4}, SPACE[2] = {0, 5}, MIC[2] = {0, 6},
                 SHIFT[2] = {1, 6}, ENTER[2] = {3, 3}, J[2] = {3, 6}, D[2] = {1, 2};

static tdeck_keymap_t st;
static kbd_report_t r;

static bool press(const int* k, bool down) { return tdeck_keymap_event(&st, k[0], k[1], down, &r); }

// The key the UI gets for one press, with `mods` held around it.
static Key tap(const int* k, const int* mod1 = nullptr, const int* mod2 = nullptr) {
  tdeck_keymap_reset(&st);
  if (mod1) press(mod1, true);
  if (mod2) press(mod2, true);
  TEST_ASSERT_TRUE(press(k, true));
  TEST_ASSERT_EQUAL(1, r.n);
  Key out{Input::translate(r.key[0], r.mods), 0, false, 0};
  out.mods = (r.mods & 0x11) ? KM_CTRL : 0;
  return out;
}

void test_alt_is_esc() { TEST_ASSERT_EQUAL(K_ESC, tap(ALT).code); }
void test_sym_space_is_tab() { TEST_ASSERT_EQUAL(K_TAB, tap(SPACE, SYM).code); }
void test_sym_enter_is_menu() { TEST_ASSERT_EQUAL(K_MENU, tap(ENTER, SYM).code); }
void test_sym_mic_is_zero() { TEST_ASSERT_EQUAL('0', tap(MIC, SYM).code); }
void test_sym_shift_j_is_down() { TEST_ASSERT_EQUAL(K_DOWN, tap(J, SYM, SHIFT).code); }

void test_mic_is_ctrl() {
  Key k = tap(D, MIC);
  TEST_ASSERT_EQUAL('d', k.code);
  TEST_ASSERT_EQUAL(KM_CTRL, k.mods);
}

void test_ball_steps_every_two_toggles() {
  tdeck_keymap_ball_t b{};
  TEST_ASSERT_EQUAL(0, tdeck_keymap_ball(&b, TDECK_KEYMAP_DOWN, 1, 2));
  TEST_ASSERT_EQUAL(1, tdeck_keymap_ball(&b, TDECK_KEYMAP_DOWN, 1, 2));
  TEST_ASSERT_EQUAL(2, tdeck_keymap_ball(&b, TDECK_KEYMAP_DOWN, 4, 2));
}

void test_ball_wobble_cancels() {
  tdeck_keymap_ball_t b{};
  TEST_ASSERT_EQUAL(0, tdeck_keymap_ball(&b, TDECK_KEYMAP_UP, 1, 2));
  TEST_ASSERT_EQUAL(0, tdeck_keymap_ball(&b, TDECK_KEYMAP_DOWN, 1, 2));
  TEST_ASSERT_EQUAL(0, tdeck_keymap_ball(&b, TDECK_KEYMAP_UP, 1, 2));
}

void test_ball_usages_are_arrows() {
  TEST_ASSERT_EQUAL(K_UP, Input::translate(tdeck_keymap_ball_usage(TDECK_KEYMAP_UP), 0));
  TEST_ASSERT_EQUAL(K_DOWN, Input::translate(tdeck_keymap_ball_usage(TDECK_KEYMAP_DOWN), 0));
  TEST_ASSERT_EQUAL(K_LEFT, Input::translate(tdeck_keymap_ball_usage(TDECK_KEYMAP_LEFT), 0));
  TEST_ASSERT_EQUAL(K_RIGHT, Input::translate(tdeck_keymap_ball_usage(TDECK_KEYMAP_RIGHT), 0));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_alt_is_esc);
  RUN_TEST(test_sym_space_is_tab);
  RUN_TEST(test_sym_enter_is_menu);
  RUN_TEST(test_sym_mic_is_zero);
  RUN_TEST(test_sym_shift_j_is_down);
  RUN_TEST(test_mic_is_ctrl);
  RUN_TEST(test_ball_steps_every_two_toggles);
  RUN_TEST(test_ball_wobble_cancels);
  RUN_TEST(test_ball_usages_are_arrows);
  return UNITY_END();
}
