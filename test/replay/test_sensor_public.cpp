// Public sensor-layer tests. These compile ONLY the sensor classification code
// (BleAdvertClassifier, WifiFrameClassifier, ReconSignatures, ReconSelection)
// and the shared model enums. They deliberately do NOT include replay.h, which
// pulls in MonitorEventLog (private application core). If this binary links
// with no core/app objects, the public sensor library is self-contained - the
// dependency direction the sensors/ split requires.
#include "check.h"
#include <string>
#include <vector>
#include "core/logic/BleAdvertClassifier.h"
using namespace layertime;
using namespace layertime::recon;

namespace {
struct Advert {
    std::string name, printed = "aa:bb:cc:dd:ee:ff";
    uint8_t bytes[6] = {0x02,0,0,0,0,0}; int8_t rssi = -60;
    std::vector<std::string> mfg; std::vector<int> uuids;
    static void mfgFn(uint8_t i, std::string &o, const void *c){ o = static_cast<const Advert*>(c)->mfg[i]; }
    static bool uuidFn(uint8_t i, uint16_t &o, const void *c){ int v = static_cast<const Advert*>(c)->uuids[i]; if(v<0) return false; o=(uint16_t)v; return true; }
    BleAdvertSource src() const { BleAdvertSource s; s.name=name.data(); s.nameLength=name.size(); s.printedAddress=printed.c_str(); s.addressBytes=bytes; s.rssi=rssi; s.manufacturerCount=(uint8_t)mfg.size(); s.manufacturer=mfgFn; s.uuidCount=(uint8_t)uuids.size(); s.uuid16=uuidFn; s.context=this; return s; }
};
int countDetect(const Advert&a, ReconTarget scan){ int n=0; classifyBleAdvert(a.src(), scan, [](const Candidate&, void*c){ (*static_cast<int*>(c))++; }, &n); return n; }
std::string mb(std::initializer_list<uint8_t> b){ return std::string(b.begin(), b.end()); }
}

void public_layer_detects_a_valid_tile(){ Advert a; a.uuids={0xFEED}; CHECK_INT(1, countDetect(a, ReconTarget::All)); }
void public_layer_rejects_apple_without_subtype(){ Advert a; a.mfg={mb({0x4C,0x00})}; CHECK_INT(0, countDetect(a, ReconTarget::AirTag)); }
void public_layer_honors_scan_scope(){ Advert a; a.uuids={0xFEED}; CHECK_INT(0, countDetect(a, ReconTarget::Meta)); }

int main(int argc, char**argv){ CHECK_MAIN(argc,argv);
  CASE(public_layer_detects_a_valid_tile);
  CASE(public_layer_rejects_apple_without_subtype);
  CASE(public_layer_honors_scan_scope);
  CHECK_SUMMARY(); }
