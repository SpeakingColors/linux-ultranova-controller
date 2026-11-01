// Tests for src/Library.h. The patches here are built from the documented layout (header, slot, name, 494 data bytes, F7).
// They check that our code follows that layout, not that it matches a real device.
#include "../src/Library.h"
#include <cstdio>
#include <cstdlib>
#include <random>
using namespace lib;

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)

static Raw makePatch(int slot, const std::string& name, unsigned seed)
{
    Raw r(headerPrefix().begin(), headerPrefix().end());
    r.push_back(uint8_t(slot)); r.push_back(0x00); r.push_back(0x2A);
    std::string n = name; n.resize(kNameLen, ' '); for (char c : n) r.push_back(uint8_t(c));
    std::mt19937 g(seed); while (r.size() < kPatchSize - 1) r.push_back(uint8_t(g() & 0x7F));
    r.push_back(0xF7); return r;
}
static std::vector<uint8_t> makeBank(int n, unsigned seed = 1)
{
    std::vector<uint8_t> out; for (int i = 0; i < n; ++i) { auto p = makePatch(i, "Patch " + std::to_string(i), seed + unsigned(i)); out.insert(out.end(), p.begin(), p.end()); } return out;
}

static void testValidate()
{
    auto p = makePatch(5, "Denim Fix", 3); std::string why;
    CHECK(p.size() == kPatchSize && validate(p.data(), p.size(), &why));
    CHECK(nameOf(p) == "Denim Fix");
    auto bad = p; bad.pop_back(); CHECK(!validate(bad.data(), bad.size(), &why) && !why.empty());
    for (size_t i : { size_t(1), size_t(4), size_t(9), size_t(11), size_t(13), size_t(14) }) { bad = p; bad[i] ^= 0x01; CHECK(!validate(bad.data(), bad.size())); }   // each header byte is checked
    bad = p; bad[kPatchSize - 1] = 0x00; CHECK(!validate(bad.data(), bad.size()));
    bad = p; bad[200] = 0x80; CHECK(!validate(bad.data(), bad.size()));
    bad = p; bad[kSlotByte] = 0x7F; CHECK(validate(bad.data(), bad.size()));      // any slot 0..127 is fine
    bad = p; bad.insert(bad.end() - 1, 0x00); CHECK(!validate(bad.data(), bad.size()));      // one byte too long
    CHECK(!validate(nullptr, 0));
}

static void testSplitAndImport()
{
    auto b = makeBank(3); std::vector<uint8_t> s = { 0x11, 0x22, 0xF8 };                     // stray bytes and a realtime byte before the bank
    s.insert(s.end(), b.begin(), b.begin() + long(kPatchSize));
    s.push_back(0xFE);                                                                       // realtime byte between messages is ignored
    s.insert(s.end(), b.begin() + long(kPatchSize), b.end());
    auto sp = splitSysex(s.data(), s.size()); CHECK(sp.messages.size() == 3 && sp.stray == 2 && sp.broken == 0);
    auto imp = importSysex(s.data(), s.size()); CHECK(imp.patches.size() == 3 && imp.rejected == 0);
    // realtime byte inside a message must not break it
    auto p = makePatch(0, "X", 1); std::vector<uint8_t> t(p.begin(), p.begin() + 100); t.push_back(0xF8); t.insert(t.end(), p.begin() + 100, p.end());
    CHECK(importSysex(t.data(), t.size()).patches.size() == 1);
    // a foreign SysEx message (identity reply) is rejected with a reason, not imported
    std::vector<uint8_t> id = { 0xF0, 0x7E, 0x00, 0x06, 0x02, 0x00, 0x20, 0x29, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0xF7 };
    auto f = importSysex(id.data(), id.size()); CHECK(f.patches.empty() && f.rejected == 1 && f.reasons.size() == 1);
    // truncated last message
    auto cut = b; cut.resize(cut.size() - 10); auto ic = importSysex(cut.data(), cut.size()); CHECK(ic.patches.size() == 2 && ic.rejected == 1);
    // two F0 in a row: the first one is dropped
    std::vector<uint8_t> dbl = { 0xF0, 0x01, 0x02 }; dbl.insert(dbl.end(), p.begin(), p.end()); auto id2 = importSysex(dbl.data(), dbl.size()); CHECK(id2.patches.size() == 1 && id2.rejected == 1);
}

static void testNames()
{
    CHECK(cleanName("  Big Saws  ") == "Big Saws");
    CHECK(cleanName("ABCDEFGHIJKLMNOPQRSTUVWXYZ") == "ABCDEFGHIJKLMNOP");
    CHECK(cleanName("caf\xC3\xA9") == "caf??");
    CHECK(cleanName("tab\there") == "tabhere");
    auto p = makePatch(9, "Old Name", 7); auto q = withName(p, "New");
    CHECK(nameOf(q) == "New" && q.size() == kPatchSize && validate(q.data(), q.size()));
    size_t diff = 0; for (size_t i = 0; i < p.size(); ++i) if (p[i] != q[i]) { ++diff; CHECK(i >= kNameOff && i < kNameOff + kNameLen); }   // only name bytes change
    CHECK(diff > 0);
    CHECK(dataKey(p) == dataKey(q));                                                          // renaming keeps the sound's identity
    auto r = p; r[kSlotByte] = 100; CHECK(dataKey(p) == dataKey(r));                          // so does renumbering
    r = p; r[300] ^= 1; CHECK(dataKey(p) != dataKey(r));
    CHECK(withName(Raw(10, 0), "x").size() == 10);                                            // not a patch: untouched
}

static void testBankEditor()
{
    auto b = makeBank(4); auto imp = importSysex(b.data(), b.size());
    BankEditor ed; for (auto& p : imp.patches) ed.bank.push_back({ p, {} });
    const Bank orig = ed.bank;
    CHECK(ed.rename(1, "Renamed") && nameOf(ed.bank[1].raw) == "Renamed");
    CHECK(!ed.rename(1, "Renamed") && !ed.rename(99, "x"));
    Meta m; m.star = true; m.category = "Lead"; m.genre = "Trance"; CHECK(ed.setMeta(0, m) && !ed.setMeta(0, m));
    CHECK(ed.move(0, 2) && nameOf(ed.bank[2].raw) == "Patch 0"); CHECK(ed.bank[2].meta.star);            // meta moves with its patch
    CHECK(!ed.move(3, 1) && !ed.move(0, -1));
    CHECK(ed.remove(0) && ed.bank.size() == 3);
    while (ed.undo()) {}                                                                      // back to the start
    CHECK(ed.bank == orig);
    int redone = 0; while (ed.redo()) ++redone; CHECK(redone == 4 && ed.bank.size() == 3);
    CHECK(!ed.redo()); ed.undo(); CHECK(ed.rename(0, "After undo")); CHECK(!ed.redo());       // a new edit clears redo
    // append respects the 128 patch limit
    BankEditor big; std::vector<Raw> many; for (int i = 0; i < 130; ++i) many.push_back(makePatch(i & 127, "n", unsigned(i)));
    CHECK(big.append(many) && big.bank.size() == kMaxBank); CHECK(!big.append(many));
    for (int i = 0; i < 300; ++i) big.rename(size_t(i % 128), "r" + std::to_string(i)); CHECK(big.hist.past.size() <= 100);
    Meta dirty; dirty.category = "Le\tad\n"; dirty.genre = std::string(60, 'g'); BankEditor e2; e2.bank.push_back({ makePatch(0, "a", 1), {} }); e2.setMeta(0, dirty);
    CHECK(e2.bank[0].meta.category == "Lead" && e2.bank[0].meta.genre.size() == 24);
}

static void testExport()
{
    auto b = makeBank(5, 10); auto imp = importSysex(b.data(), b.size()); Bank bank; for (auto& p : imp.patches) bank.push_back({ p, {} });
    std::swap(bank[0], bank[4]);                                                              // reorder, then export
    auto out = exportSysex(bank); CHECK(out.size() == 5 * kPatchSize);
    auto back = importSysex(out.data(), out.size()); CHECK(back.patches.size() == 5 && back.rejected == 0);
    for (size_t i = 0; i < 5; ++i)
    {
        CHECK(back.patches[i][kSlotByte] == i);                                               // renumbered in list order
        CHECK(dataKey(back.patches[i]) == dataKey(bank[i].raw) && nameOf(back.patches[i]) == nameOf(bank[i].raw));
        Raw a = back.patches[i], c = bank[i].raw; a[kSlotByte] = 0; c[kSlotByte] = 0; CHECK(a == c);   // nothing but the slot byte differs
    }
    CHECK(exportSysex(bank, 3).size() == 2 * kPatchSize && exportSysex(bank, 0, 2).size() == 2 * kPatchSize);
    Bank big; for (int i = 0; i < 200; ++i) big.push_back({ makePatch(0, "x", unsigned(i)), {} }); CHECK(exportSysex(big).size() == kMaxBank * kPatchSize);
}

static void testSidecar()
{
    Bank b; for (int i = 0; i < 3; ++i) b.push_back({ makePatch(i, "P" + std::to_string(i), unsigned(40 + i)), {} });
    b[1].meta = { true, "Pad", "Ambient" }; b[2].meta = { false, "Bass", "" };
    const std::string t = metaText(b); Bank c = b; for (auto& e : c) e.meta = {};
    applyMetaText(c, t); CHECK(c == b);
    std::swap(c[0], c[2]); for (auto& e : c) e.meta = {}; applyMetaText(c, t); CHECK(c[0].meta.category == "Bass" && c[1].meta.star);   // follows the sound, not the position
    Bank d = b; for (auto& e : d) e.meta = {}; applyMetaText(d, "garbage\nzz\t1\ta\tb\n\n# c\n"); CHECK(d[0].meta == Meta{});       // malformed lines ignored
    Bank dup; dup.push_back({ makePatch(0, "A", 5), {} }); dup.push_back({ makePatch(1, "B", 5), {} });   // two identical sounds
    dup[0].meta.category = "Lead"; dup[1].meta.category = "Pad"; Bank e = dup; for (auto& x : e) x.meta = {}; applyMetaText(e, metaText(dup)); CHECK(e == dup);
}

static void testDisk()
{
    std::error_code ec; const fs::path root = fs::temp_directory_path() / ("unova_lib_test_" + std::to_string(std::rand())); fs::remove_all(root, ec);
    fs::create_directories(root / "My Patches" / "Trance leads", ec); fs::create_directories(root / ".hidden", ec);
    auto bytes = makeBank(6, 70); auto imp = importSysex(bytes.data(), bytes.size()); Bank bank; for (auto& p : imp.patches) bank.push_back({ p, {} });
    bank[3].meta = { true, "Lead", "Trance" };
    const fs::path file = root / "My Patches" / "Trance leads" / "Mine.syx";
    CHECK(writeBank(file, bank) && fs::exists(metaPath(file)) && !fs::exists(file.string() + ".tmp"));
    auto l = readBank(file); CHECK(l.ok && l.bank.size() == 6 && l.info.rejected == 0);
    for (size_t i = 0; i < 6; ++i) { Raw a = l.bank[i].raw, c = bank[i].raw; a[kSlotByte] = 0; c[kSlotByte] = 0; CHECK(a == c); }
    CHECK(l.bank[3].meta.star && l.bank[3].meta.category == "Lead");
    CHECK(!readBank(root / "missing.syx").ok);
    writeFileAtomic(root / "My Patches" / "notes.txt", "x", 1); writeFileAtomic(root / "My Patches" / "Other.SYX", bytes.data(), bytes.size());
    std::vector<Node> nodes; scan(root, 0, nodes);
    CHECK(nodes.size() == 4);                                                                 // My Patches, Trance leads, Mine.syx, Other.SYX (hidden and .txt skipped)
    CHECK(nodes[0].name == "My Patches" && !nodes[0].isBank && nodes[1].name == "Trance leads" && nodes[1].depth == 1 && nodes[2].name == "Mine" && nodes[2].isBank && nodes[2].depth == 2 && nodes[3].name == "Other" && nodes[3].depth == 1);
    // backup and restore
    const fs::path where = root.parent_path() / (root.filename().string() + "_bk"); fs::create_directories(where, ec); std::string err;
    auto bk = backup(root, where, err); CHECK(!bk.empty() && err.empty() && fs::exists(bk / "My Patches" / "Trance leads" / "Mine.syx"));
    CHECK(backup(root / "nope", where, err).empty() && !err.empty());
    auto r0 = restore(bk, root); CHECK(r0.err.empty() && r0.copied == 0 && r0.renamed == 0 && r0.same >= 4);            // restoring over identical files changes nothing
    bank[0] = { makePatch(0, "Changed", 999), {} }; writeBank(file, bank);                                           // user edits, then restores the older backup
    auto r1 = restore(bk, root); CHECK(r1.err.empty() && r1.renamed >= 1);
    CHECK(fs::exists(root / "My Patches" / "Trance leads" / "Mine (restored).syx"));
    CHECK(nameOf(readBank(file).bank[0].raw) == "Changed");                                                           // the edited bank is untouched
    CHECK(nameOf(readBank(root / "My Patches" / "Trance leads" / "Mine (restored).syx").bank[0].raw) == "Patch 0");
    fs::remove(file, ec); fs::remove(metaPath(file), ec); CHECK(restore(bk, root).copied >= 2); CHECK(fs::exists(file));   // a deleted bank comes back
    CHECK(!restore(root / "nope", root).err.empty());
    CHECK(safeFileName("a/b:c*?") == "a_b_c__" && safeFileName("  ..") == "Untitled" && safeFileName(std::string(100, 'x')).size() == 60);
    CHECK(uniquePath(root, "My Patches", "").filename() == "My Patches 2");
    fs::remove_all(root, ec); fs::remove_all(where, ec);
}

static void testFuzz()
{
    std::mt19937 g(12345);
    for (int round = 0; round < 3000; ++round)
    {
        std::vector<uint8_t> d(size_t(g() % 1500)); for (auto& x : d) x = uint8_t(g());
        if (round % 3 == 0) { auto p = makePatch(1, "Fuzz", g()); d.insert(d.begin() + long(d.empty() ? 0 : g() % d.size()), p.begin(), p.end()); }   // a valid patch hidden in noise
        auto imp = importSysex(d.data(), d.size());
        for (auto& p : imp.patches) { CHECK(p.size() == kPatchSize); if (fails) return; }
        Bank b; for (auto& p : imp.patches) b.push_back({ p, {} });
        auto out = exportSysex(b); auto again = importSysex(out.data(), out.size()); CHECK(again.patches.size() == std::min(imp.patches.size(), kMaxBank));
        std::string t(d.begin(), d.end()); applyMetaText(b, t);                                // random text as a sidecar must not crash
        if (fails) return;
    }
}

int main()
{
    testValidate(); testSplitAndImport(); testNames(); testBankEditor(); testExport(); testSidecar(); testDisk(); testFuzz();
    if (fails) { std::printf("%d check(s) failed\n", fails); return 1; }
    std::printf("library tests passed\n"); return 0;
}
