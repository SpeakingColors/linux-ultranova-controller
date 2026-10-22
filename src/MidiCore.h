// MidiCore: pure C++ (no JUCE). Parameter map parsing, MIDI encoding and decoding for CC and NRPN,
// following Novation's "UltraNova Program Parameter MIDI Assignments" (see docs/MIDI-IMPLEMENTATION.md).
// Only things listed in map/params.csv are ever sent. Nothing here invents SysEx.
// Audio thread safe: Decoder and Outbox never allocate after construction.
#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace ctl
{
// CC: one controller number. NRPN: 7 bit data. NRPN14: 14 bit data (dataMSB then dataLSB, only ClockBPM in the source).
// Pair: two CCs that form one value; the manual does not say how, so it is not sent.
enum class Kind { CC, NRPN, Pair, NRPN14 };

struct Param
{
    std::string id, group, name;
    Kind kind = Kind::CC;
    int a = 0;        // CC number, or NRPN MSB, or first CC of a pair
    int b = 0;        // NRPN LSB, or second CC of a pair
    int minV = 0, maxV = 127, defV = 0;
    bool rangeFromManual = false;   // false: 0 to 127 assumed, not printed in the manual
    bool hwVerified = false;        // true only after the value was tested on the real device
    bool isNrpn() const { return kind == Kind::NRPN || kind == Kind::NRPN14; }
    bool isGlobal() const { return group == "Global"; }   // audio setup, not a patch value: never automated
};

struct ParseResult { std::vector<Param> params; std::vector<std::string> errors; };

inline std::vector<std::string> splitCsv(const std::string& line)
{
    std::vector<std::string> out; std::string cur; bool q = false;
    for (size_t i = 0; i < line.size(); ++i)
    {
        char c = line[i];
        if (q) { if (c == '"') { if (i + 1 < line.size() && line[i + 1] == '"') { cur += '"'; ++i; } else q = false; } else cur += c; }
        else if (c == '"') q = true;
        else if (c == ',') { out.push_back(cur); cur.clear(); }
        else if (c != '\r') cur += c;
    }
    out.push_back(cur); return out;
}

inline bool toInt(const std::string& s, int& v)
{
    if (s.empty() || s.size() > 6) return false;
    size_t i = 0; if (s[0] == '-') i = 1; if (i >= s.size()) return false;
    for (size_t k = i; k < s.size(); ++k) if (s[k] < '0' || s[k] > '9') return false;
    v = std::stoi(s); return true;
}

// Columns: id,group,name,kind,a,b,min,max,def,range_src,hw
inline ParseResult parseMap(const std::string& text)
{
    ParseResult r; std::istringstream in(text); std::string line; int ln = 0; std::set<std::string> ids; std::set<std::string> addr;
    while (std::getline(in, line))
    {
        ++ln; if (line.empty() || line[0] == '#' || line.rfind("id,", 0) == 0) continue;
        auto f = splitCsv(line); auto err = [&](const std::string& m) { r.errors.push_back("line " + std::to_string(ln) + ": " + m); };
        if (f.size() != 11) { err("expected 11 columns"); continue; }
        Param p; p.id = f[0]; p.group = f[1]; p.name = f[2];
        if (p.id.empty() || p.name.empty()) { err("empty id or name"); continue; }
        if (f[3] == "cc") p.kind = Kind::CC; else if (f[3] == "nrpn") p.kind = Kind::NRPN; else if (f[3] == "pair") p.kind = Kind::Pair;
        else if (f[3] == "nrpn14") p.kind = Kind::NRPN14; else { err("unknown kind"); continue; }
        if (!toInt(f[4], p.a) || !toInt(f[5], p.b) || !toInt(f[6], p.minV) || !toInt(f[7], p.maxV) || !toInt(f[8], p.defV)) { err("bad number"); continue; }
        if (p.a < 0 || p.a > 127 || p.b < 0 || p.b > 127) { err("number outside 0..127"); continue; }
        const int top = p.kind == Kind::NRPN14 ? 16383 : 127;
        if (p.minV < 0 || p.maxV > top || p.minV > p.maxV || p.defV < p.minV || p.defV > p.maxV) { err("bad range"); continue; }
        p.rangeFromManual = f[9] == "manual"; p.hwVerified = f[10] == "1";
        if (!ids.insert(p.id).second) { err("duplicate id " + p.id); continue; }
        const char space = p.kind == Kind::CC ? 'c' : p.kind == Kind::Pair ? 'p' : 'n';   // NRPN and NRPN14 share one address space
        std::string key = std::string(1, space) + ":" + std::to_string(p.a) + ":" + std::to_string(p.kind == Kind::CC ? 0 : p.b);
        if (!addr.insert(key).second) { err("duplicate address for " + p.id); continue; }
        r.params.push_back(p);
    }
    return r;
}

// One MIDI message on the wire (channel messages here; len is 3 for CC, 2 for program change).
struct Msg { uint8_t status = 0, d1 = 0, d2 = 0; uint8_t len = 3; };

inline Msg cc(int ch /*1..16*/, int num, int val)
{
    Msg m; m.status = uint8_t(0xB0 | ((std::clamp(ch, 1, 16) - 1) & 15)); m.d1 = uint8_t(std::clamp(num, 0, 127)); m.d2 = uint8_t(std::clamp(val, 0, 127)); return m;
}
inline Msg programChange(int ch, int prog) { Msg m; m.status = uint8_t(0xC0 | ((std::clamp(ch, 1, 16) - 1) & 15)); m.d1 = uint8_t(std::clamp(prog, 0, 127)); m.len = 2; return m; }

// Bank select (CC 0 = MSB, CC 32 = LSB) followed by a program change. What the bank numbers mean is not in our source.
inline std::vector<Msg> selectPatch(int ch, int bankMsb, int bankLsb, int prog)
{
    return { cc(ch, 0, bankMsb), cc(ch, 32, bankLsb), programChange(ch, prog) };
}

// NRPN with 7 bit data: CC 99 (MSB), CC 98 (LSB), CC 6 (data). The MSB is always sent, also for 0, so the message
// is correct whatever the device was left in. No null message is sent afterwards (the device resets its MSB itself).
inline std::vector<Msg> nrpn(int ch, int msb, int lsb, int val)
{
    return { cc(ch, 99, msb), cc(ch, 98, lsb), cc(ch, 6, val) };
}
// High resolution: nrpnMSB, nrpnLSB, dataMSB, dataLSB in this order (the device applies the value when dataLSB arrives).
inline std::vector<Msg> nrpn14(int ch, int msb, int lsb, int val14)
{
    val14 = std::clamp(val14, 0, 16383);
    return { cc(ch, 99, msb), cc(ch, 98, lsb), cc(ch, 6, val14 >> 7), cc(ch, 38, val14 & 127) };
}

// What to send for one parameter value. Pair parameters send nothing (not verified how the two CCs combine).
inline std::vector<Msg> encode(const Param& p, int ch, int val)
{
    val = std::clamp(val, p.minV, p.maxV);
    if (p.kind == Kind::CC) return { cc(ch, p.a, val) };
    if (p.kind == Kind::NRPN) return nrpn(ch, p.a, p.b, val);
    if (p.kind == Kind::NRPN14) return nrpn14(ch, p.a, p.b, val);
    return {};
}

// Packed bytes: a field of `width` bits starting at bit `lo` (the source lists bit positions, e.g. LFO "4:Fade 3:DelayMulti ...").
// Other bits of the byte are not known to us, so callers keep the whole byte and change only their field.
inline int getBits(int byteValue, int lo, int width) { return (byteValue >> lo) & ((1 << width) - 1); }
inline int setBits(int byteValue, int lo, int width, int v)
{
    const int mask = ((1 << width) - 1) << lo; return ((byteValue & ~mask) | ((v << lo) & mask)) & 127;
}
// Touch and solo controls: the unit sends 0 or 127, and treats 0..63 as off, 64..127 as on.
inline bool isOn(int v) { return v >= 64; }

struct Event { int paramIndex = -1; int value = 0; };

// Turns incoming CC messages from the device into parameter events.
// Follows the source: nrpnMSB is 0 at power-up and again after every data entry (CC 6), so the panel only sends
// CC 98 + CC 6 for the first 128 NRPNs. 14 bit parameters need CC 6 then CC 38 and apply when CC 38 arrives.
class Decoder
{
public:
    explicit Decoder(const std::vector<Param>& map) : params(map), ccIdx(128, -1), nrpnIdx(16384, -1)
    {
        for (size_t i = 0; i < params.size(); ++i)
        {
            const auto& p = params[i];
            if (p.kind == Kind::CC) ccIdx[size_t(p.a)] = int(i);
            else if (p.isNrpn()) nrpnIdx[size_t(p.a << 7 | p.b)] = int(i);
        }
    }
    void reset() { msb = 0; lsb = -1; wait14 = -1; hi14 = 0; }
    std::optional<Event> feedCC(int num, int val)
    {
        val &= 127;
        switch (num)
        {
            case 99: msb = val; lsb = -1; wait14 = -1; return std::nullopt;
            case 98: lsb = val; wait14 = -1; return std::nullopt;
            case 6:
            {
                const int m = msb; msb = 0;          // the device resets nrpnMSB after every data entry
                if (lsb < 0) return std::nullopt;
                const int i = nrpnIdx[size_t(m << 7 | lsb)];
                if (i < 0) { wait14 = -1; return std::nullopt; }
                if (params[size_t(i)].kind == Kind::NRPN14) { wait14 = i; hi14 = val; return std::nullopt; }
                wait14 = -1;
                return Event{ i, std::clamp(val, params[size_t(i)].minV, params[size_t(i)].maxV) };
            }
            case 38:
                if (wait14 < 0) return std::nullopt;   // a second dataLSB, or one with no pending 14 bit value, is ignored
                { const int i = wait14; wait14 = -1; return Event{ i, std::clamp(hi14 << 7 | val, params[size_t(i)].minV, params[size_t(i)].maxV) }; }
            default: break;
        }
        const int i = num >= 0 && num < 128 ? ccIdx[size_t(num)] : -1;   // other CCs do not disturb a running NRPN sequence
        if (i < 0) return std::nullopt;
        return Event{ i, std::clamp(val, params[size_t(i)].minV, params[size_t(i)].maxV) };
    }
private:
    const std::vector<Param>& params;
    std::vector<int> ccIdx, nrpnIdx;
    int msb = 0, lsb = -1, wait14 = -1, hi14 = 0;
};

// Outgoing queue: the latest value per parameter wins, parameters go out in turn (no parameter starves), and a
// parameter's messages are never split, so an NRPN sequence is never cut in half by the per-block limit.
class Outbox
{
public:
    explicit Outbox(const std::vector<Param>& map) : params(map), val(map.size(), 0), dirty(map.size(), 0) {}
    void setChannel(int ch) { channel = std::clamp(ch, 1, 16); }
    void set(size_t i, int v) { if (i < params.size() && params[i].kind != Kind::Pair) { val[i] = v; dirty[i] = 1; } }
    void setAll(const std::vector<int>& values) { for (size_t i = 0; i < values.size() && i < params.size(); ++i) set(i, values[i]); }
    bool empty() const { for (auto d : dirty) if (d) return false; return true; }
    size_t pending() const { size_t n = 0; for (auto d : dirty) n += d; return n; }
    // Writes at most maxMsgs messages into out and returns how many were written.
    size_t drain(Msg* out, size_t maxMsgs)
    {
        size_t n = 0; const size_t total = params.size(); if (!total) return 0;
        for (size_t k = 0; k < total; ++k)
        {
            const size_t i = (start + k) % total; if (!dirty[i]) continue;
            const size_t need = params[i].kind == Kind::NRPN14 ? 4 : params[i].kind == Kind::NRPN ? 3 : 1;
            if (n + need > maxMsgs) { if (n == 0) continue; else break; }   // keep the group whole; try the rest next time
            for (const auto& m : encode(params[i], channel, val[i])) out[n++] = m;
            dirty[i] = 0; start = (i + 1) % total;
        }
        return n;
    }
private:
    const std::vector<Param>& params; std::vector<int> val; std::vector<uint8_t> dirty; size_t start = 0; int channel = 1;
};

// Universal Device Inquiry (standard MIDI): ask any device who it is. The reply is parsed, not assumed.
inline std::array<uint8_t, 6> identityRequest() { return { 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 }; }
struct Identity
{
    bool valid = false; std::array<uint8_t, 3> manufacturer{}; int mfrLen = 0; int family = 0, member = 0; std::array<uint8_t, 4> version{};
    bool isNovation() const { return valid && mfrLen == 3 && manufacturer[0] == 0x00 && manufacturer[1] == 0x20 && manufacturer[2] == 0x29; }
};
inline Identity parseIdentityReply(const uint8_t* d, size_t n)
{
    Identity id;
    if (n < 15 || d[0] != 0xF0 || d[1] != 0x7E || d[3] != 0x06 || d[4] != 0x02 || d[n - 1] != 0xF7) return id;
    size_t k = 5; int len = d[k] == 0 ? 3 : 1;
    if (n != size_t(5 + len + 4 + 4 + 1)) return id;
    for (int i = 0; i < len; ++i) id.manufacturer[size_t(i)] = d[k++];
    id.mfrLen = len;
    for (size_t i = k; i + 1 < n; ++i) if (d[i] > 0x7F) return id;      // data bytes only
    id.family = d[k] | d[k + 1] << 7; id.member = d[k + 2] | d[k + 3] << 7; k += 4;
    for (size_t i = 0; i < 4; ++i) id.version[i] = d[k + i];
    id.valid = true; return id;
}
}
