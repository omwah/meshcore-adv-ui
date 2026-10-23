#include "ui/gfx/emoji.h"
#include "ui/gfx/utf8.h"

namespace ui {

namespace {

constexpr int MAXLEN = 12;   // elements examined; the table's maxlen never exceeds it

inline bool isSelector(uint32_t cp) { return cp == 0xFE0F || cp == 0xFE0E; }

// Compare the first n code points of `a` with sequence `q`.
int compare(const uint32_t* a, int n, const mc_emoji_seqs_t* t, const mc_seq_t& q) {
  const uint32_t* b = t->cps + q.off;
  for (int i = 0; i < n && i < q.len; i++)
    if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
  return n == q.len ? 0 : (n < q.len ? -1 : 1);
}

// Index of the sequence equal to a[0..n), or -1.
int find(const mc_emoji_seqs_t* t, const uint32_t* a, int n) {
  int lo = 0, hi = (int)t->nseqs - 1;
  while (lo <= hi) {
    int mid = (lo + hi) >> 1;
    int c = compare(a, n, t, t->seqs[mid]);
    if (c == 0) return mid;
    if (c < 0) hi = mid - 1; else lo = mid + 1;
  }
  return -1;
}

// Whether any sequence starts with cp: the cheap test that keeps ordinary text
// from decoding ahead.
bool startsWith(const mc_emoji_seqs_t* t, uint32_t cp) {
  int lo = 0, hi = (int)t->nseqs - 1;
  while (lo <= hi) {
    int mid = (lo + hi) >> 1;
    uint32_t v = t->cps[t->seqs[mid].off];
    if (v == cp) return true;
    if (v < cp) lo = mid + 1; else hi = mid - 1;
  }
  return false;
}

}  // namespace

int emojiMatch(const mc_emoji_t* e, const char* s, int* glyph) {
  const mc_emoji_seqs_t* t = e->seqs;
  if (!t || !t->nseqs) return 0;
  uint32_t cps[MAXLEN];
  int ends[MAXLEN];                        // byte offset just past element i and its selectors
  int n = 0, pos = 0;
  const int maxn = t->maxlen < MAXLEN ? t->maxlen : MAXLEN;
  while (n < maxn) {
    uint32_t cp;
    int used = utf8Decode(s + pos, nullptr, &cp);
    if (cp == 0 || used == 0) break;
    if (n == 0 && (isSelector(cp) || !startsWith(t, cp))) return 0;
    if (cp == REPLACEMENT) break;          // damaged input ends a sequence
    if (isSelector(cp)) {                  // belongs to the element before it
      if (n == 0) break;
      pos += used;
      ends[n - 1] = pos;
      continue;
    }
    cps[n] = cp;
    pos += used;
    ends[n] = pos;
    n++;
  }
  for (int k = n; k >= 1; k--) {
    int i = find(t, cps, k);
    if (i >= 0) {
      *glyph = t->seqs[i].glyph;
      return ends[k - 1];
    }
  }
  return 0;
}

void emojiDecode(const mc_emoji_t* e, int glyph, int rows, uint8_t* out) {
  const uint8_t* in = e->bitmaps + e->offs[glyph];
  int left = rows * e->w;
  while (left > 0) {
    int run = (*in >> 4) + 1;
    const uint8_t v = *in++ & 15;
    if (run > left) run = left;
    left -= run;
    while (run--) *out++ = v;
  }
}

}  // namespace ui
