// Compiles MeshCore's stock simple_repeater (and the CLI/ACL helpers only it
// uses) with its classes renamed, so it can run next to companion nodes.
#define MyMesh RepeaterMesh
#define NodePrefs RepeaterNodePrefs
#include "../../.deps/meshcore/src/helpers/CommonCLI.cpp"
#include "../../.deps/meshcore/src/helpers/ClientACL.cpp"
#include "../../.deps/meshcore/examples/simple_repeater/MyMesh.cpp"
#undef MyMesh
#undef NodePrefs

#include <LittleFS.h>
#include <helpers/IdentityStore.h>
#include "sim_repeater.h"
#include "sim_node.h"   // SimRNG

namespace {

class Impl : public SimRepeater {
public:
  Impl(SimMedium& m, int id, const std::string& name, const std::string& dir, uint32_t epoch, const std::string& pw)
      : _radio(m, id), _rtc(epoch), _rng(0x85EBCA6Bu * (uint32_t)(id + 1)), _name(name), _dir(dir), _pw(pw) {
    _fs.setRoot(dir);
    _fs.format();
    m.add(&_radio);
    _mesh.reset(new RepeaterMesh(board, _radio, _ms, _rng, _rtc, _tables));
    select();
    IdentityStore store(_fs, "/identity");
    if (!store.load("_main", _mesh->self_id)) {
      _mesh->self_id = mesh::LocalIdentity(&_rng);
      store.save("_main", _mesh->self_id);
    }
    _mesh->begin(&_fs);
    RepeaterNodePrefs* p = _mesh->getNodePrefs();
    snprintf(p->node_name, sizeof p->node_name, "%s", _name.c_str());
    snprintf(p->password, sizeof p->password, "%s", _pw.c_str());
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
  std::unique_ptr<RepeaterMesh> _mesh;
  std::string _name, _dir, _pw;
};

}  // namespace

std::unique_ptr<SimRepeater> SimRepeater::create(SimMedium& m, int id, const std::string& name, const std::string& dir,
                                                 uint32_t epoch, const std::string& pw) {
  return std::unique_ptr<SimRepeater>(new Impl(m, id, name, dir, epoch, pw));
}
