#include "../src/MidiCore.h"
#include "SimUltraNova.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)
using namespace ctl;

static void feed(SimUltraNova& s, const std::vector<Msg>& v) { for (auto& m : v) s.receive(m.status, m.d1, m.d2); }
static const Param* findNrpn(const std::vector<Param>& ps, int msb, int lsb) { for (auto& p : ps) if (p.isNrpn() && p.a == msb && p.b == lsb) return &p; return nullptr; }
static const Param* findCC(const std::vector<Param>& ps, int n) { for (auto& p : ps) if (p.kind == Kind::CC && p.a == n) return &p; return nullptr; }

static void basics()
{
    auto m = cc(1, 74, 100); CHECK(m.status == 0xB0 && m.d1 == 74 && m.d2 == 100 && m.len == 3);
    m = cc(16, 1, 300); CHECK(m.status == 0xBF && m.d2 == 127);
    m = cc(0, 1, -5); CHECK(m.status == 0xB0 && m.d2 == 0);
    auto n = nrpn(3, 0, 14, 9); CHECK(n.size() == 3 && n[0].d1 == 99 && n[0].d2 == 0 && n[1].d1 == 98 && n[1].d2 == 14 && n[2].d1 == 6 && n[2].d2 == 9 && n[0].status == 0xB2);
    auto h = nrpn14(1, 2, 63, 12345);   // 12345 = 96 * 128 + 57
    CHECK(h.size() == 4 && h[0].d1 == 99 && h[0].d2 == 2 && h[1].d1 == 98 && h[1].d2 == 63 && h[2].d1 == 6 && h[2].d2 == 96 && h[3].d1 == 38 && h[3].d2 == 57);
    h = nrpn14(1, 2, 63, 99999); CHECK(h[2].d2 == 127 && h[3].d2 == 127);   // clamped to 14 bits
    auto pc = selectPatch(2, 1, 3, 17); CHECK(pc.size() == 3 && pc[0].d1 == 0 && pc[0].d2 == 1 && pc[1].d1 == 32 && pc[1].d2 == 3 && pc[2].status == 0xC1 && pc[2].d1 == 17 && pc[2].len == 2);
    CHECK(programChange(1, 500).d1 == 127);
    // packed bits
    CHECK(getBits(0b10110, 1, 2) == 0b11); CHECK(setBits(0b10110, 1, 2, 0) == 0b10000); CHECK(setBits(0, 4, 1, 1) == 16); CHECK(setBits(127, 5, 2, 0) == 31); CHECK(setBits(0, 0, 7, 255) == 127);
    CHECK(!isOn(63) && isOn(64) && isOn(127) && !isOn(0));
}

static void parsing()
{
    const char* csv = "id,group,name,kind,a,b,min,max,def,range_src,hw\n"
        "cc_5,Voice,Glide Time,cc,5,0,0,127,0,manual,0\n"
        "nrpn_0_2,Voice,Voice Mode,nrpn,0,2,0,4,3,manual,0\n"
        "cc14_14_46,Osc,Coarse,pair,14,46,0,127,0,assumed,0\n"
        "nrpn_2_63,Clock,ClockBPM,nrpn14,2,63,0,16383,100,manual,0\n";
    auto r = parseMap(csv); CHECK(r.errors.empty()); CHECK(r.params.size() == 4);
    CHECK(r.params[1].kind == Kind::NRPN && r.params[1].maxV == 4); CHECK(r.params[3].kind == Kind::NRPN14 && r.params[3].maxV == 16383);
    CHECK(encode(r.params[2], 1, 5).empty());                  // pairs are not sent
    CHECK(encode(r.params[1], 1, 99).back().d2 == 4);          // clamped to the range
    CHECK(encode(r.params[3], 1, 16383).size() == 4);
    CHECK(!parseMap("a,b\nx").errors.empty());
    CHECK(!parseMap("cc_1,g,n,cc,200,0,0,127,0,manual,0\n").errors.empty());
    CHECK(!parseMap("cc_1,g,n,cc,1,0,0,127,0,manual,0\ncc_2,g,n2,cc,1,0,0,127,0,manual,0\n").errors.empty());
    CHECK(!parseMap("cc_1,g,n,cc,1,0,5,3,0,manual,0\n").errors.empty());
    CHECK(!parseMap("cc_1,g,n,wat,1,0,0,127,0,manual,0\n").errors.empty());
    CHECK(!parseMap("n1,g,n,nrpn,1,0,0,200,0,manual,0\n").errors.empty());                         // 7 bit NRPN may not exceed 127
    CHECK(!parseMap("n1,g,n,nrpn,1,5,0,127,0,manual,0\nn2,g,n2,nrpn14,1,5,0,16383,0,manual,0\n").errors.empty());   // NRPN and NRPN14 share addresses
    CHECK(parseMap("c1,g,n,cc,5,0,0,127,0,manual,0\nn1,g,n,nrpn,5,0,0,127,0,manual,0\n").errors.empty());   // CC 5 and NRPN 5:0 do not clash
}

static void decoding()
{
    const char* csv = "id,group,name,kind,a,b,min,max,def,range_src,hw\n"
        "cc_5,Voice,Glide Time,cc,5,0,0,127,0,manual,0\n"
        "nrpn_0_2,Voice,Voice Mode,nrpn,0,2,0,4,3,manual,0\n"
        "nrpn_1_7,Fx,Thing,nrpn,1,7,0,127,0,manual,0\n"
        "nrpn_2_63,Clock,ClockBPM,nrpn14,2,63,0,16383,100,manual,0\n";
    auto r = parseMap(csv); Decoder d(r.params);
    auto e = d.feedCC(5, 64); CHECK(e && e->paramIndex == 0 && e->value == 64);
    // the panel sends NRPN 0:xxx without the MSB: this must work from power-up
    CHECK(!d.feedCC(98, 2)); e = d.feedCC(6, 3); CHECK(e && e->paramIndex == 1 && e->value == 3);
    // after the data entry the MSB is 0 again, so a bare data entry addresses 0:2 and not 1:7
    CHECK(!d.feedCC(99, 1)); CHECK(!d.feedCC(98, 7)); e = d.feedCC(6, 9); CHECK(e && e->paramIndex == 2 && e->value == 9);
    e = d.feedCC(6, 1); CHECK(!e);                           // 0:7 is not in this test map
    CHECK(!d.feedCC(98, 2)); e = d.feedCC(6, 99); CHECK(e && e->paramIndex == 1 && e->value == 4);   // clamped to the range
    { Decoder fresh(r.params); CHECK(!fresh.feedCC(6, 1)); }                       // data entry with no NRPN selected is ignored
    CHECK(!d.feedCC(77, 1));                                  // unknown CC
    // other controllers inside a running sequence do not disturb it
    d.reset(); CHECK(!d.feedCC(99, 1)); CHECK(!d.feedCC(98, 7)); e = d.feedCC(5, 10); CHECK(e && e->paramIndex == 0); e = d.feedCC(6, 20); CHECK(e && e->paramIndex == 2 && e->value == 20);
    // 14 bit: applied only when dataLSB arrives, and a second dataLSB is ignored
    d.reset(); d.feedCC(99, 2); d.feedCC(98, 63); CHECK(!d.feedCC(6, 3)); e = d.feedCC(38, 5); CHECK(e && e->paramIndex == 3 && e->value == 3 * 128 + 5);
    CHECK(!d.feedCC(38, 9));
    CHECK(!d.feedCC(38, 1));                                  // dataLSB with nothing pending
    d.reset(); d.feedCC(99, 2); d.feedCC(98, 63); d.feedCC(6, 127); e = d.feedCC(38, 127); CHECK(e && e->value == 16383);
    // a new address before the LSB cancels the pending 14 bit value
    d.reset(); d.feedCC(99, 2); d.feedCC(98, 63); d.feedCC(6, 1); d.feedCC(98, 2); CHECK(!d.feedCC(38, 1));
    // an NRPN message that is not in the map is dropped quietly
    d.reset(); d.feedCC(99, 9); d.feedCC(98, 9); CHECK(!d.feedCC(6, 9)); CHECK(!d.feedCC(38, 9));
}

static void map_tests(const std::vector<Param>& ps)
{
    int cc_n = 0, nr = 0, n14 = 0, pr = 0, gl = 0; for (auto& p : ps) { cc_n += p.kind == Kind::CC; nr += p.kind == Kind::NRPN; n14 += p.kind == Kind::NRPN14; pr += p.kind == Kind::Pair; gl += p.isGlobal(); }
    std::printf("map: %zu entries (%d CC, %d NRPN, %d NRPN 14 bit, %d pair, %d global)\n", ps.size(), cc_n, nr, n14, pr, gl);
    CHECK(cc_n == 107); CHECK(nr == 374); CHECK(n14 == 1); CHECK(gl == 32);
    // spot checks against the addresses printed in the source (typed here independently of tools/gen_map.py)
    struct K { int m, l; const char* has; int lo, hi; };
    const K ks[] = { {0, 0, "FiltEnv Velocity", 0, 127}, {0, 1, "FiltEnv Attack", 0, 127}, {0, 13, "FiltEnv Touch Trigger", 0, 127}, {0, 14, "Env3 Delay", 0, 127},
        {0, 69, "Env6 Touch Trigger", 0, 127}, {0, 70, "Lfo1 Waveform", 0, 127}, {0, 78, "Lfo1 Packed", 0, 127}, {0, 96, "Lfo3 Packed", 0, 127}, {0, 99, "Fx Slot1 Effect", 0, 127},
        {0, 104, "Bass EQ Frequency", 0, 127}, {0, 121, "Compress2 Auto-Gain", 0, 127}, {1, 0, "Distort1 Type", 0, 127}, {1, 5, "Distort2 Output", 0, 127},
        {1, 6, "Delay1 Time", 0, 127}, {1, 17, "Delay2 Slew", 0, 127}, {1, 24, "Phaser/Chorus1 Type", 0, 127}, {1, 47, "Phaser/Chorus4 Delay", 0, 127},
        {1, 48, "Gator Packed", 0, 127}, {1, 54, "Gator Delay", 0, 127}, {1, 55, "Vocoder Packed", 0, 127}, {1, 60, "Vocoder Spectrum Magnify", 0, 127},
        {1, 61, "Arpeggiator Packed", 0, 127}, {1, 70, "Arpeggiator Quantise", 0, 127}, {1, 78, "Chord Transpose", 0, 127}, {1, 80, "ModMatrix Velocity Slew", 0, 127},
        {1, 83, "ModMatrix1 Source1", 0, 127}, {1, 87, "ModMatrix1 Destination", 0, 127}, {1, 123, "ModMatrix9 Source1", 0, 127}, {1, 127, "ModMatrix9 Destination", 0, 127},
        {2, 0, "ModMatrix10 Source1", 0, 127}, {2, 54, "ModMatrix20 Destination", 0, 127}, {2, 63, "ClockBPM", 0, 16383}, {5, 0, "Gator Level L1", 0, 7},
        {5, 15, "Gator Level L16", 0, 7}, {5, 16, "Gator Level R1", 0, 7}, {5, 31, "Gator Level R16", 0, 7}, {60, 0, "Touch 1", 0, 127}, {60, 7, "Touch 8", 0, 127},
        {61, 0, "Solo Osc1", 0, 127}, {61, 5, "Solo Noise", 0, 127}, {64, 0, "Protect", 0, 127}, {64, 4, "Global MIDI Channel", 0, 127}, {64, 28, "Input1 Gain", 0, 127},
        {64, 44, "Phones Balance", 0, 127} };
    for (auto& k : ks) { auto* p = findNrpn(ps, k.m, k.l); CHECK(p && p->name.find(k.has) != std::string::npos && p->minV == k.lo && p->maxV == k.hi); if (!p) std::printf("  missing NRPN %d:%d (%s)\n", k.m, k.l, k.has); }
    // addresses the source lists as spare or packed specials must NOT be exposed
    for (int l : { 122, 123, 124, 125, 126, 127 }) CHECK(!findNrpn(ps, 0, l));
    CHECK(!findNrpn(ps, 1, 68)); CHECK(!findNrpn(ps, 1, 79)); for (int l = 55; l <= 62; ++l) CHECK(!findNrpn(ps, 2, l));
    for (int l : { 1, 5, 10, 12, 13, 20, 27, 45, 127 }) CHECK(!findNrpn(ps, 64, l)); CHECK(!findNrpn(ps, 65, 0));
    // mod matrix: 20 slots of 5, consecutive addresses, names match the slot
    for (int k = 1; k <= 20; ++k)
    {
        const int msb = k <= 9 ? 1 : 2, base = k <= 9 ? 83 + 5 * (k - 1) : 5 * (k - 10);
        const char* nm[5] = { "Source1", "Source2", "Touch Enable", "Depth", "Destination" };
        for (int i = 0; i < 5; ++i) { auto* p = findNrpn(ps, msb, base + i); CHECK(p && p->name == "ModMatrix" + std::to_string(k) + " " + nm[i]); }
    }
    // standard CC numbers keep their meaning and the free ones are absent
    for (int n : { 0, 6, 32, 38, 96, 97, 98, 99, 100, 101, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127 }) CHECK(!findCC(ps, n));
    CHECK(findCC(ps, 74) && findCC(ps, 74)->name == "Filter1 Frequency"); CHECK(findCC(ps, 71)->name == "Filter1 Resonance"); CHECK(findCC(ps, 1)->name == "Mod wheel");
}

static void sim_roundtrip(const std::vector<Param>& ps)
{
    // every entry, at min, mid, max and its default: our messages reach the simulated device as the right value,
    // and our own decoder reads the same messages back as the same parameter and value.
    int tested = 0;
    for (auto& p : ps)
    {
        if (p.kind == Kind::Pair) continue;
        for (int v : { p.minV, (p.minV + p.maxV) / 2, p.maxV, p.defV })
        {
            for (int ch : { 1, 10, 16 })
            {
                auto msgs = encode(p, ch, v);
                for (auto& m : msgs) CHECK((m.status & 0x0F) == ch - 1);
                SimUltraNova s2; feed(s2, msgs); CHECK(s2.bad == 0);
                if (p.kind == Kind::CC) CHECK(s2.ccSet[p.a] && s2.cc[p.a] == v);
                else CHECK(s2.nrpnSet[p.a][p.b] && s2.nrpn[p.a][p.b] == v);
                Decoder d2(ps); std::optional<Event> ev; for (auto& m : msgs) { auto r = d2.feedCC(m.d1, m.d2); if (r) ev = r; }
                CHECK(ev && ps[size_t(ev->paramIndex)].id == p.id && ev->value == v);
                ++tested;
            }
        }
    }
    std::printf("round trips through the simulated device: %d\n", tested);
    // sending the whole map in a row: no entry disturbs another
    SimUltraNova s; std::map<std::string, int> sent; int k = 0;
    for (auto& p : ps) { if (p.kind == Kind::Pair) continue; const int v = p.minV + (k++ * 37) % (p.maxV - p.minV + 1); sent[p.id] = v; feed(s, encode(p, 1, v)); }
    for (auto& p : ps) { if (p.kind == Kind::Pair) continue; const int got = p.kind == Kind::CC ? s.cc[p.a] : s.nrpn[p.a][p.b]; CHECK(got == sent[p.id]); }
    CHECK(s.bad == 0);
    // standard "two message" panel style for NRPN 0:xxx reaches our decoder (no CC 99)
    for (auto& p : ps) if (p.kind == Kind::NRPN && p.a == 0)
    { Decoder d(ps); d.feedCC(98, p.b); auto e = d.feedCC(6, p.maxV); CHECK(e && ps[size_t(e->paramIndex)].id == p.id && e->value == p.maxV); }
    // the same for 14 bit values with the full four messages
    { auto* p = findNrpn(ps, 2, 63); Decoder d(ps); d.feedCC(99, 2); d.feedCC(98, 63); d.feedCC(6, 50); auto e = d.feedCC(38, 100); CHECK(p && e && e->value == 50 * 128 + 100); }
}

static void outbox_tests(const std::vector<Param>& ps)
{
    Outbox ob(ps); ob.setChannel(3); std::vector<Msg> buf(2000);
    CHECK(ob.empty()); CHECK(ob.drain(buf.data(), 100) == 0);
    const size_t iCC = size_t(findCC(ps, 74) - ps.data()), iN = size_t(findNrpn(ps, 0, 1) - ps.data()), iH = size_t(findNrpn(ps, 2, 63) - ps.data());
    // latest value wins
    ob.set(iCC, 10); ob.set(iCC, 90); CHECK(ob.pending() == 1); size_t n = ob.drain(buf.data(), 100); CHECK(n == 1 && buf[0].d1 == 74 && buf[0].d2 == 90 && buf[0].status == 0xB2); CHECK(ob.empty());
    // message counts per kind
    ob.set(iN, 5); n = ob.drain(buf.data(), 100); CHECK(n == 3); ob.set(iH, 9000); n = ob.drain(buf.data(), 100); CHECK(n == 4 && buf[2].d2 == 9000 / 128 && buf[3].d2 == 9000 % 128);
    // groups are never cut: budget 4 with three NRPNs pending sends one group of 3, then the rest later
    ob.set(iN, 1); ob.set(size_t(findNrpn(ps, 0, 2) - ps.data()), 2); ob.set(size_t(findNrpn(ps, 0, 3) - ps.data()), 3);
    n = ob.drain(buf.data(), 4); CHECK(n == 3 && ob.pending() == 2); n = ob.drain(buf.data(), 4); CHECK(n == 3); n = ob.drain(buf.data(), 4); CHECK(n == 3 && ob.empty());
    // a budget smaller than any group sends nothing and keeps the data
    ob.set(iH, 1); CHECK(ob.drain(buf.data(), 3) == 0 && ob.pending() == 1); CHECK(ob.drain(buf.data(), 4) == 4);
    // everything dirty: drained in turn, nothing starves, nothing is cut, total content correct
    std::vector<int> all(ps.size()); for (size_t i = 0; i < ps.size(); ++i) all[i] = ps[i].maxV; ob.setAll(all);
    SimUltraNova sim; int rounds = 0; size_t total = 0;
    while (!ob.empty() && rounds < 1000) { n = ob.drain(buf.data(), 48); CHECK(n > 0 && n <= 48); for (size_t i = 0; i < n; ++i) sim.receive(buf[i].status, buf[i].d1, buf[i].d2); total += n; ++rounds; }
    CHECK(ob.empty()); CHECK(sim.bad == 0);
    for (auto& p : ps) { if (p.kind == Kind::Pair) continue; const int got = p.kind == Kind::CC ? sim.cc[p.a] : sim.nrpn[p.a][p.b]; CHECK(got == p.maxV); }
    std::printf("outbox: whole map in %d blocks of at most 48 messages (%zu messages)\n", rounds, total);
    // out-of-range index and pairs are ignored
    Outbox o2(ps); o2.set(999999, 1); CHECK(o2.empty());
    std::vector<Param> empty; Outbox o3(empty); CHECK(o3.drain(buf.data(), 10) == 0);
}

static void identity_tests()
{
    auto q = identityRequest(); CHECK(q[0] == 0xF0 && q[1] == 0x7E && q[2] == 0x7F && q[3] == 0x06 && q[4] == 0x01 && q[5] == 0xF7);
    const uint8_t nov[] = { 0xF0, 0x7E, 0x00, 0x06, 0x02, 0x00, 0x20, 0x29, 0x01, 0x02, 0x03, 0x04, 0x31, 0x2E, 0x30, 0x35, 0xF7 };
    auto id = parseIdentityReply(nov, sizeof nov); CHECK(id.valid && id.isNovation() && id.mfrLen == 3 && id.family == (0x01 | 0x02 << 7) && id.member == (0x03 | 0x04 << 7) && id.version[0] == 0x31);
    const uint8_t one[] = { 0xF0, 0x7E, 0x7F, 0x06, 0x02, 0x41, 0x01, 0x00, 0x02, 0x00, 1, 2, 3, 4, 0xF7 };
    id = parseIdentityReply(one, sizeof one); CHECK(id.valid && !id.isNovation() && id.mfrLen == 1);
    CHECK(!parseIdentityReply(nov, sizeof nov - 1).valid);                       // no F7
    uint8_t bad[sizeof nov]; std::memcpy(bad, nov, sizeof nov); bad[9] = 0x90; CHECK(!parseIdentityReply(bad, sizeof bad).valid);   // data byte with the high bit set
    std::memcpy(bad, nov, sizeof nov); bad[4] = 0x01; CHECK(!parseIdentityReply(bad, sizeof bad).valid);   // a request, not a reply
    CHECK(!parseIdentityReply(nullptr, 0).valid);
    // random bytes never crash and almost never parse
    unsigned seed = 12345; auto rnd = [&] { seed = seed * 1664525u + 1013904223u; return seed >> 8; };
    for (int i = 0; i < 20000; ++i) { uint8_t b[24]; size_t n = rnd() % 24; for (size_t k = 0; k < n; ++k) b[k] = uint8_t(rnd()); (void)parseIdentityReply(b, n); }
}

static void fuzz(const std::vector<Param>& ps)
{
    unsigned seed = 99; auto rnd = [&] { seed = seed * 1664525u + 1013904223u; return seed >> 8; };
    Decoder d(ps); int events = 0;
    for (int i = 0; i < 400000; ++i)
    {
        const int num = (rnd() % 3 == 0) ? (std::vector<int>{ 6, 38, 98, 99 })[rnd() % 4] : int(rnd() % 300) - 20;
        auto e = d.feedCC(num, int(rnd() % 400) - 100);
        if (e) { ++events; CHECK(e->paramIndex >= 0 && size_t(e->paramIndex) < ps.size()); const auto& p = ps[size_t(e->paramIndex)]; CHECK(e->value >= p.minV && e->value <= p.maxV); }
    }
    Outbox ob(ps); std::vector<Msg> buf(512);
    for (int i = 0; i < 20000; ++i)
    {
        ob.set(rnd() % (ps.size() + 5), int(rnd() % 20000) - 100); if (rnd() % 7 == 0) ob.setChannel(int(rnd() % 40) - 5);
        size_t n = ob.drain(buf.data(), rnd() % 60);
        for (size_t k = 0; k < n; ++k) { CHECK((buf[k].status & 0xF0) == 0xB0 && buf[k].d1 < 128 && buf[k].d2 < 128); }
    }
    std::printf("fuzz: %d events from random input, all inside their ranges\n", events);
}

int main(int argc, char** argv)
{
    basics(); parsing(); decoding(); identity_tests();
    if (argc > 1)
    {
        std::ifstream f(argv[1]); std::stringstream ss; ss << f.rdbuf(); auto real = parseMap(ss.str());
        for (auto& er : real.errors) std::printf("map error: %s\n", er.c_str());
        CHECK(real.errors.empty()); CHECK(real.params.size() > 400);
        map_tests(real.params); sim_roundtrip(real.params); outbox_tests(real.params); fuzz(real.params);
        int hw = 0; for (auto& p : real.params) hw += p.hwVerified; std::printf("verified on hardware: %d of %zu\n", hw, real.params.size());
    }
    std::printf(fails ? "%d failures\n" : "all core tests passed\n", fails);
    return fails ? 1 : 0;
}
