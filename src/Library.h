// Library: the file side of the UltraNova patch librarian. Pure C++17, no JUCE, so it is tested by tests/run.sh.
//
// What it knows, and where that comes from:
//  - A patch dump is one SysEx message of 526 bytes. The header layout (bytes 0..14), the 16 character ASCII name that
//    starts at byte 15 and the closing F7 were verified on a real 128 patch bank file (see docs/MIDI-IMPLEMENTATION.md).
//  - Bytes 31..524 are patch data. They are NEVER read, decoded or changed here. Novation's byte layout is not public in
//    the documents we have, so nothing in this file guesses it.
//  - The only bytes this code ever writes are the slot number (byte 12, renumbered when a bank is exported) and the name.
//  - Category, genre and favourite star are not in the dump. They live in a small sidecar file next to each bank.
//  - Nothing here talks to the device. Grab, send and audition wait for a capture of the Novation Librarian
//    (the SysEx request message is not in any document we have).
#pragma once
#include <algorithm>
#include <cctype>
#include <array>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace lib
{
namespace fs = std::filesystem;
using Raw = std::vector<uint8_t>;

constexpr size_t kPatchSize = 526, kSlotByte = 12, kNameOff = 15, kNameLen = 16, kDataOff = 31, kDataEnd = 525;   // data is [31, 525)
constexpr size_t kMaxBank = 128;

inline const std::array<uint8_t, 12>& headerPrefix()
{
    static const std::array<uint8_t, 12> h = { 0xF0, 0x00, 0x20, 0x29, 0x03, 0x01, 0x7F, 0x01, 0x00, 0x09, 0x04, 0x00 };
    return h;
}

// ---------- one patch dump ----------
inline bool validate(const uint8_t* d, size_t n, std::string* why = nullptr)
{
    auto no = [&](const char* m) { if (why) *why = m; return false; };
    if (n != kPatchSize) return no("not 526 bytes long");
    for (size_t i = 0; i < headerPrefix().size(); ++i) if (d[i] != headerPrefix()[i]) return no("header is not an UltraNova patch dump");
    if (d[13] != 0x00 || d[14] != 0x2A) return no("header end bytes differ");
    if (d[kPatchSize - 1] != 0xF7) return no("does not end with F7");
    for (size_t i = 1; i + 1 < n; ++i) if (d[i] > 0x7F) return no("data byte above 127");
    return true;
}

inline std::string nameOf(const Raw& r)
{
    std::string s;
    if (r.size() == kPatchSize) for (size_t i = 0; i < kNameLen; ++i) s += char(r[kNameOff + i]);
    while (!s.empty() && s.back() == ' ') s.pop_back();
    return s;
}

// Keeps printable ASCII only (the name field is 16 ASCII characters), cuts to 16 and pads with spaces.
inline std::string cleanName(const std::string& in)
{
    std::string s;
    for (unsigned char c : in) { if (c >= 0x20 && c <= 0x7E) s += char(c); else if (c >= 0x80) s += '?'; }
    while (!s.empty() && s.front() == ' ') s.erase(s.begin());
    if (s.size() > kNameLen) s.resize(kNameLen);
    while (!s.empty() && s.back() == ' ') s.pop_back();
    return s;
}

inline Raw withName(Raw r, const std::string& name)
{
    if (r.size() != kPatchSize) return r;
    std::string s = cleanName(name); s.resize(kNameLen, ' ');
    for (size_t i = 0; i < kNameLen; ++i) r[kNameOff + i] = uint8_t(s[i]);
    return r;
}

// Identifies the sound itself (patch data only: not the slot, not the name), so a sidecar entry follows the patch when a
// bank is reordered or renamed, even outside this plugin. FNV-1a, 64 bit.
inline uint64_t dataKey(const Raw& r)
{
    uint64_t h = 1469598103934665603ULL;
    if (r.size() == kPatchSize) for (size_t i = kDataOff; i < kDataEnd; ++i) { h ^= r[i]; h *= 1099511628211ULL; }
    return h;
}
inline std::string hex64(uint64_t v) { char b[20]; std::snprintf(b, sizeof b, "%016llx", (unsigned long long) v); return b; }

// ---------- SysEx streams ----------
// Splits a byte stream into F0..F7 messages. Bytes outside a message and realtime bytes (F8..FF) are skipped.
// A message cut short by another status byte is dropped.
struct Split { std::vector<Raw> messages; int stray = 0, broken = 0; };
inline Split splitSysex(const uint8_t* d, size_t n)
{
    Split s; Raw cur; bool in = false;
    for (size_t i = 0; i < n; ++i)
    {
        const uint8_t b = d[i];
        if (b >= 0xF8) continue;
        if (b == 0xF0) { if (in) ++s.broken; cur.assign(1, b); in = true; }
        else if (b == 0xF7) { if (in) { cur.push_back(b); s.messages.push_back(cur); cur.clear(); in = false; } else ++s.stray; }
        else if (b >= 0x80) { if (in) { ++s.broken; cur.clear(); in = false; } else ++s.stray; }
        else if (in) cur.push_back(b);
        else ++s.stray;
    }
    if (in) ++s.broken;
    return s;
}

struct Imported { std::vector<Raw> patches; int rejected = 0, stray = 0; std::vector<std::string> reasons; };   // reasons: first few, no repeats
inline Imported importSysex(const uint8_t* d, size_t n)
{
    Imported r; auto sp = splitSysex(d, n); r.stray = sp.stray; r.rejected = sp.broken;
    if (sp.broken) r.reasons.push_back("an unfinished SysEx message was skipped");
    for (auto& m : sp.messages)
    {
        std::string why;
        if (validate(m.data(), m.size(), &why)) r.patches.push_back(m);
        else { ++r.rejected; if (r.reasons.size() < 4 && std::find(r.reasons.begin(), r.reasons.end(), why) == r.reasons.end()) r.reasons.push_back(why); }
    }
    return r;
}

// ---------- entries, banks, undo ----------
struct Meta { bool star = false; std::string category, genre; bool operator==(const Meta& o) const { return star == o.star && category == o.category && genre == o.genre; } };
struct Entry { Raw raw; Meta meta; bool operator==(const Entry& o) const { return raw == o.raw && meta == o.meta; } };
using Bank = std::vector<Entry>;

inline std::string cleanTag(const std::string& in)
{
    std::string s; for (unsigned char c : in) if (c >= 0x20 && c != 0x7F && c < 0x80) s += char(c);
    while (!s.empty() && s.front() == ' ') s.erase(s.begin());
    while (!s.empty() && s.back() == ' ') s.pop_back();
    if (s.size() > 24) s.resize(24);
    return s;
}

// Serialises a bank for sending or sharing: one message per patch, slot numbers 0, 1, 2 ... in list order.
// The slot byte is the only byte of the patch that changes.
inline std::vector<uint8_t> exportSysex(const Bank& b, size_t from = 0, size_t count = kMaxBank)
{
    std::vector<uint8_t> out; size_t slot = 0;
    for (size_t i = from; i < b.size() && slot < count && slot < kMaxBank; ++i, ++slot)
    { Raw r = b[i].raw; if (r.size() != kPatchSize) continue; r[kSlotByte] = uint8_t(slot); out.insert(out.end(), r.begin(), r.end()); }
    return out;
}

template <class T> struct History
{
    std::vector<T> past, future; size_t limit = 100;
    void record(const T& before) { past.push_back(before); if (past.size() > limit) past.erase(past.begin()); future.clear(); }
    bool canUndo() const { return !past.empty(); }
    bool canRedo() const { return !future.empty(); }
    bool undo(T& cur) { if (past.empty()) return false; future.push_back(cur); cur = past.back(); past.pop_back(); return true; }
    bool redo(T& cur) { if (future.empty()) return false; past.push_back(cur); cur = future.back(); future.pop_back(); return true; }
    void clear() { past.clear(); future.clear(); }
};

// Edits on a bank. Each one records the state before it, so Undo and Redo work for every change.
struct BankEditor
{
    Bank bank; History<Bank> hist;
    bool rename(size_t i, const std::string& name)
    { if (i >= bank.size()) return false; Raw n = withName(bank[i].raw, name); if (n == bank[i].raw) return false; hist.record(bank); bank[i].raw = n; return true; }
    bool setMeta(size_t i, const Meta& m)
    { if (i >= bank.size()) return false; Meta c; c.star = m.star; c.category = cleanTag(m.category); c.genre = cleanTag(m.genre); if (c == bank[i].meta) return false; hist.record(bank); bank[i].meta = c; return true; }
    bool move(size_t i, int delta)
    { if (i >= bank.size()) return false; const long j = long(i) + delta; if (j < 0 || j >= long(bank.size()) || j == long(i)) return false; hist.record(bank); std::swap(bank[i], bank[size_t(j)]); return true; }
    bool remove(size_t i) { if (i >= bank.size()) return false; hist.record(bank); bank.erase(bank.begin() + long(i)); return true; }
    bool append(const std::vector<Raw>& patches)
    {
        if (patches.empty() || bank.size() >= kMaxBank) return false; hist.record(bank);
        for (auto& p : patches) { if (bank.size() >= kMaxBank) break; bank.push_back({ p, {} }); }
        return true;
    }
    bool undo() { return hist.undo(bank); }
    bool redo() { return hist.redo(bank); }
};

// ---------- files ----------
inline std::string safeFileName(const std::string& in)
{
    std::string s; for (unsigned char c : in) { if (c < 0x20 || c == 0x7F || std::string("/\\:*?\"<>|").find(char(c)) != std::string::npos) s += '_'; else s += char(c); }
    while (!s.empty() && (s.back() == ' ' || s.back() == '.')) s.pop_back();
    while (!s.empty() && (s.front() == ' ' || s.front() == '.')) s.erase(s.begin());
    if (s.size() > 60) s.resize(60);
    return s.empty() ? std::string("Untitled") : s;
}

inline fs::path uniquePath(const fs::path& dir, const std::string& stem, const std::string& ext)
{
    fs::path p = dir / (stem + ext); std::error_code ec;
    for (int i = 2; fs::exists(p, ec) && i < 10000; ++i) p = dir / (stem + " " + std::to_string(i) + ext);
    return p;
}

inline bool readFile(const fs::path& p, std::vector<uint8_t>& out)
{
    std::ifstream f(p, std::ios::binary); if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()); return true;
}
// Writes to a temporary file and renames it over the target, so a crash never leaves half a bank behind.
inline bool writeFileAtomic(const fs::path& p, const void* data, size_t n)
{
    const fs::path tmp = p.string() + ".tmp";
    { std::ofstream f(tmp, std::ios::binary | std::ios::trunc); if (!f) return false; f.write(static_cast<const char*>(data), std::streamsize(n)); if (!f) return false; }
    std::error_code ec; fs::rename(tmp, p, ec);
    if (ec) { fs::remove(p, ec); ec.clear(); fs::rename(tmp, p, ec); }
    if (ec) { fs::remove(tmp, ec); return false; }
    return true;
}

inline fs::path metaPath(const fs::path& bankFile) { return fs::path(bankFile.string() + ".meta"); }

// Sidecar: one line per patch, in bank order: key <TAB> star <TAB> category <TAB> genre. Matched back by key, not by position.
inline std::string metaText(const Bank& b)
{
    std::string s = "# Studio 87 UltraNova library: category, genre and favourites for the patches of the .syx file next to this one.\n";
    for (auto& e : b) s += hex64(dataKey(e.raw)) + "\t" + (e.meta.star ? "1" : "0") + "\t" + cleanTag(e.meta.category) + "\t" + cleanTag(e.meta.genre) + "\n";
    return s;
}
inline void applyMetaText(Bank& b, const std::string& text)
{
    std::multimap<std::string, Meta> m; std::istringstream in(text); std::string line;
    while (std::getline(in, line))
    {
        if (line.empty() || line[0] == '#') continue;
        std::vector<std::string> f; std::string cur; for (char c : line) { if (c == '\t') { f.push_back(cur); cur.clear(); } else if (c != '\r') cur += c; } f.push_back(cur);
        if (f.size() != 4 || f[0].size() != 16) continue;
        Meta x; x.star = f[1] == "1"; x.category = cleanTag(f[2]); x.genre = cleanTag(f[3]); m.emplace(f[0], x);
    }
    for (auto& e : b) { auto it = m.find(hex64(dataKey(e.raw))); if (it != m.end()) { e.meta = it->second; m.erase(it); } }
}

struct Loaded { Bank bank; Imported info; bool ok = false; };
inline Loaded readBank(const fs::path& file)
{
    Loaded l; std::vector<uint8_t> bytes; if (!readFile(file, bytes)) return l;
    l.info = importSysex(bytes.data(), bytes.size()); l.ok = true;
    for (auto& p : l.info.patches) { if (l.bank.size() >= kMaxBank) { ++l.info.rejected; break; } l.bank.push_back({ p, {} }); }
    std::vector<uint8_t> mt; if (readFile(metaPath(file), mt)) applyMetaText(l.bank, std::string(mt.begin(), mt.end()));
    return l;
}
inline bool writeBank(const fs::path& file, const Bank& b)
{
    const auto bytes = exportSysex(b); if (!writeFileAtomic(file, bytes.data(), bytes.size())) return false;
    const std::string m = metaText(b); return writeFileAtomic(metaPath(file), m.data(), m.size());
}

// ---------- the library folder ----------
struct Node { std::string name; fs::path path; bool isBank = false; int depth = 0; };
inline bool lessNoCase(const std::string& a, const std::string& b)
{ return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), [](unsigned char x, unsigned char y) { return std::tolower(x) < std::tolower(y); }); }
inline bool isSyx(const fs::path& p)
{ std::string e = p.extension().string(); std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c) { return char(std::tolower(c)); }); return e == ".syx"; }

// All folders and banks under root, depth first: folders (sorted) before banks (sorted) at each level.
inline void scan(const fs::path& dir, int depth, std::vector<Node>& out)
{
    std::error_code ec; std::vector<fs::path> dirs, files;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
    {
        const auto& p = it->path(); const std::string n = p.filename().string(); if (n.empty() || n[0] == '.') continue;
        std::error_code e2; if (fs::is_directory(p, e2)) dirs.push_back(p); else if (isSyx(p)) files.push_back(p);
    }
    auto byName = [](const fs::path& a, const fs::path& b) { return lessNoCase(a.filename().string(), b.filename().string()); };
    std::sort(dirs.begin(), dirs.end(), byName); std::sort(files.begin(), files.end(), byName);
    for (auto& d : dirs) { out.push_back({ d.filename().string(), d, false, depth }); if (depth < 8) scan(d, depth + 1, out); }
    for (auto& f : files) out.push_back({ f.stem().string(), f, true, depth });
}

inline std::string stamp()
{
    std::time_t t = std::time(nullptr); char b[32]; std::strftime(b, sizeof b, "%Y%m%d-%H%M%S", std::gmtime(&t)); return b;
}

// Copies the whole library into <where>/UltraNova-library-<time>. Returns the new folder, or empty with the reason in err.
inline fs::path backup(const fs::path& root, const fs::path& where, std::string& err)
{
    std::error_code ec; if (!fs::is_directory(root, ec)) { err = "the library folder does not exist"; return {}; }
    const fs::path dest = where / ("UltraNova-library-" + stamp());
    fs::create_directories(dest, ec); if (ec) { err = "cannot create " + dest.string(); return {}; }
    fs::copy(root, dest, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    if (ec) { err = ec.message(); return {}; }
    return dest;
}

// Copies a backup folder into the library. Nothing is ever deleted or overwritten: when a file with the same name
// but different bytes exists, the restored one is added as "<name> (restored)".
struct Restored { int copied = 0, renamed = 0, same = 0; std::string err; };
inline Restored restore(const fs::path& backupDir, const fs::path& root)
{
    Restored r; std::error_code ec; if (!fs::is_directory(backupDir, ec)) { r.err = "backup folder not found"; return r; }
    for (fs::recursive_directory_iterator it(backupDir, ec), end; !ec && it != end; it.increment(ec))
    {
        if (!it->is_regular_file()) continue;
        const fs::path rel = fs::relative(it->path(), backupDir, ec); fs::path target = root / rel;
        fs::create_directories(target.parent_path(), ec);
        if (fs::exists(target, ec))
        {
            std::vector<uint8_t> a, b; readFile(it->path(), a); readFile(target, b);
            if (a == b) { ++r.same; continue; }
            // a bank's sidecar follows its bank: restore the pair under the same new name
            std::string ext = target.extension().string(), stem = target.stem().string();
            if (ext == ".meta") { fs::path bank = target; bank.replace_extension(); stem = bank.stem().string() + " (restored)"; target = target.parent_path() / (stem + ".syx.meta"); }
            else target = uniquePath(target.parent_path(), stem + " (restored)", ext);
            ++r.renamed;
        }
        else ++r.copied;
        fs::copy_file(it->path(), target, fs::copy_options::overwrite_existing, ec);
        if (ec) { r.err = ec.message(); return r; }
    }
    return r;
}

inline const std::vector<std::string>& categories() { static const std::vector<std::string> c = { "Lead", "Pad", "Bass", "Arp", "Pluck", "Keys", "Movement", "FX", "Drum", "Other" }; return c; }
inline const std::vector<std::string>& genres() { static const std::vector<std::string> g = { "Trance", "Hard Dance", "Hardstyle", "House", "Techno", "Dubstep", "DnB", "Classic", "Ambient", "Other" }; return g; }
}
