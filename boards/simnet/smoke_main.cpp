// Two stock companion nodes on the simulated medium: adverts, a DM and its
// ACK. Exercises MeshCore end to end on the host. Run: simnet --smoke
#include <stdio.h>
#include "sim_node.h"

int simnetSmoke() {
  SimMedium medium;
  SimNode a(medium, 0, "Alpha", "sim-data/smoke/a", 1791727500);
  SimNode b(medium, 1, "Bravo", "sim-data/smoke/b", 1791727500);
  medium.link(0, 1, 7.5f);
  a.begin();
  b.begin();

  unsigned long t = 0;
  auto run = [&](unsigned long ms) {
    for (unsigned long end = t + ms; t < end; t += 5) {
      sim_setMillis(t);
      a.loop();
      b.loop();
    }
  };
  a.select(); a.mesh().advert();
  b.select(); b.mesh().advert();
  run(5000);
  a.select();
  printf("A contacts: %d, B contacts: %d\n", a.mesh().getNumContacts(), (b.select(), b.mesh().getNumContacts()));
  a.select();
  ContactInfo* to = a.mesh().searchContactsByPrefix("Bravo");
  if (!to) { printf("FAIL: Bravo not a contact of Alpha\n"); return 1; }
  uint32_t ack = 0, timeout = 0;
  int r = a.mesh().sendMessage(*to, 1791727600, 0, "hello bravo", ack, timeout);
  printf("send result %d, expected ack %08x, timeout %u ms\n", r, ack, timeout);
  run(10000);
  printf("packets on air: %u\n", medium.packets());
  return 0;
}
