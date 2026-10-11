#include "sim_node.h"

static SimRNG* s_identityRng = nullptr;
static uint32_t s_boots = 0;   // every boot draws different randomness, like real hardware

// MeshCore asks the variant for a fresh identity on first boot.
mesh::LocalIdentity radio_new_identity() { return mesh::LocalIdentity(s_identityRng); }

SimNode::SimNode(SimMedium& medium, int id, const std::string& name, const std::string& dir, uint32_t epoch,
                 AbstractUITask* ui, bool fresh)
    : _id(id), _name(name), _dir(dir), _radio(medium, id), _rtc(epoch), _rng(0x9E3779B9u * (uint32_t)(id + 1) + 0x7F4A7C15u * ++s_boots) {
  _fs.setRoot(dir);
  if (fresh) _fs.format();
  else _fs.mkdir("/");
  medium.add(&_radio);
  _store = new DataStore(_fs, _rtc);
  _mesh = new MyMesh(_radio, _rng, _rtc, _tables, *_store, ui);
}

SimNode::~SimNode() {
  delete _mesh;
  delete _store;
}

void SimNode::select() {
  SimRadio::current = &_radio;
  SimRTC::current = &_rtc;
  LittleFS.setRoot(_dir);
  s_identityRng = &_rng;
}

void SimNode::begin() {
  select();
  _store->begin();
  _mesh->begin(false);
  NodePrefs* p = _mesh->getNodePrefs();
  snprintf(p->node_name, sizeof p->node_name, "%s", _name.c_str());
  _mesh->savePrefs();
  _mesh->startInterface(_serial);
}

void SimNode::loop() {
  select();
  _mesh->loop();
}
