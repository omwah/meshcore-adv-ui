// MeshCore's stock simple_room_server for the simulation, renamed like the
// repeater (repeater_unit.cpp, which also provides the shared CommonCLI and
// ClientACL code these headers refer to).
#define MyMesh RoomMesh
#define NodePrefs RepeaterNodePrefs
#include "../../.deps/meshcore/examples/simple_room_server/MyMesh.cpp"
#undef MyMesh
#undef NodePrefs

#include <LittleFS.h>
#include <helpers/IdentityStore.h>
#include "sim_room.h"
#include "sim_node.h"   // SimRNG

namespace {

class Impl : public SimRoom {
public:
  Impl(SimMedium& m, int id, const std::string& name, const std::string& dir, uint32_t epoch, const std::string& pw)
      : _radio(m, id), _rtc(epoch), _rng(0xC2B2AE35u * (uint32_t)(id + 1)), _dir(dir) {
    _fs.setRoot(dir);
    _fs.format();
    m.add(&_radio);
    _mesh.reset(new RoomMesh(board, _radio, _ms, _rng, _rtc, _tables));
    select();
    IdentityStore store(_fs, "/identity");
    if (!store.load("_main", _mesh->self_id)) {
      _mesh->self_id = mesh::LocalIdentity(&_rng);
      store.save("_main", _mesh->self_id);
    }
    _mesh->begin(&_fs);
    RepeaterNodePrefs* p = _mesh->getNodePrefs();
    snprintf(p->node_name, sizeof p->node_name, "%s", name.c_str());
    snprintf(p->password, sizeof p->password, "%s", pw.c_str());
    snprintf(p->guest_password, sizeof p->guest_password, "%s", "");
    _mesh->savePrefs();
  }
  void select() override {
    SimRadio::current = &_radio;
    SimRTC::current = &_rtc;
    LittleFS.setRoot(_dir);
  }
  void loop() override {
    select();
    _mesh->loop();
  }
  void advert(bool flood) override {
    select();
    _mesh->sendSelfAdvertisement(0, flood);
  }

private:
  class Ms : public mesh::MillisecondClock {
  public:
    unsigned long getMillis() override { return millis(); }
  } _ms;
  SimRadio _radio;
  SimRTC _rtc;
  SimRNG _rng;
  SimpleMeshTables _tables;
  fs::FS _fs;
  std::unique_ptr<RoomMesh> _mesh;
  std::string _dir;
};

}  // namespace

std::unique_ptr<SimRoom> SimRoom::create(SimMedium& m, int id, const std::string& name, const std::string& dir,
                                         uint32_t epoch, const std::string& pw) {
  return std::unique_ptr<SimRoom>(new Impl(m, id, name, dir, epoch, pw));
}
