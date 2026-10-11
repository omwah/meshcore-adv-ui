// simnet: the UI on real MeshCore nodes over a simulated radio medium.
#ifndef PIO_UNIT_TESTING
#include <string.h>
int simnetSmoke();

int main(int argc, char** argv) {
  for (int i = 1; i < argc; i++)
    if (!strcmp(argv[i], "--smoke")) return simnetSmoke();
  return simnetSmoke();
}
#endif
