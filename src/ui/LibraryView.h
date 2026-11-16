// The live Librarian page: toolbar, folder tree and patch list drawn over the mockup artwork (assets/ui/lib.png).
// Every rectangle comes from the mockup (tools/export_mockup.py writes them into assets/ui/lib.json), so the real window
// and the mockup stay the same page. File work is in src/Library.h (tested without JUCE).
// Not wired yet: Grab, Reset (Init), audition and send. They need the SysEx request message of the UltraNova, which no
// document we have contains; they are drawn dim and say why. Nothing is ever sent to the device from this page.
#pragma once
#include <functional>
#include <map>
#include <memory>
#include <vector>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Library.h"

namespace unova
{
inline juce::String jstr(const std::string& s) { return juce::String::fromUTF8(s.c_str()); }
inline juce::File toFile(const lib::fs::path& p) { return juce::File(juce::String(juce::CharPointer_UTF8(p.u8string().c_str()))); }
inline lib::fs::path toPath(const juce::File& f) { return lib::fs::u8path(f.getFullPathName().toRawUTF8()); }

inline juce::File defaultLibraryRoot()
{
#if JUCE_LINUX
    juce::File base = juce::SystemStats::getEnvironmentVariable("XDG_DATA_HOME", {}).isNotEmpty() ? juce::File(juce::SystemStats::getEnvironmentVariable("XDG_DATA_HOME", {}))
                                                                                                   : juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(".local/share");
#else
    juce::File base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
#endif
    return base.getChildFile("Studio 87").getChildFile("UltraNova Controller").getChildFile("library");
}

class LibraryView : public juce::Component, public juce::TooltipClient, public juce::FileDragAndDropTarget
{
public:
    using Rect = juce::Rectangle<float>;
    std::function<void(const juce::File&)> onRootChanged;   // the plugin keeps the chosen folder in its saved state

    LibraryView(const std::map<juce::String, Rect>& geometry, const juce::File& root, std::function<juce::Font(float)> fontMaker)
        : g(geometry), font(std::move(fontMaker)), root(toPath(root))
    {
        setWantsKeyboardFocus(true);
        addAndMakeVisible(search);
        search.setTextToShowWhenEmpty("Type here to search", juce::Colour(0xff6C7385));
        search.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0B0E16)); search.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff444444));
        search.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff6F80AC)); search.setColour(juce::TextEditor::textColourId, juce::Colour(0xffDDDDEE));
        search.setFont(font(12.0f)); search.setMultiLine(false); search.setReturnKeyStartsNewLine(false);
        search.onTextChange = [this] { rebuildView(); repaint(); };
        if (auto r = at("search"); !r.isEmpty()) search.setBounds(r.toNearestInt());
        reload(); status("Library: " + jstr(this->root.u8string()));
    }

    bool hitTest(int x, int y) override
    {
        const juce::Point<float> p((float) x, (float) y);
        for (auto* k : { "prev", "next", "grab", "reset", "newfolder", "newbank", "undo", "redo", "search", "tree", "table" }) if (at(k).contains(p)) return true;
        return false;
    }

    // ----- drawing -----
    void paint(juce::Graphics& gr) override
    {
        drawButton(gr, "prev", juce::String::fromUTF8("\xE2\x88\x92 Previous"), true, false); drawButton(gr, "next", "+ Next", true, false);
        drawButton(gr, "grab", juce::String::fromUTF8("\xE2\x96\xB6 Grab"), false, true); drawButton(gr, "reset", "Reset (Init)", false, false);
        drawButton(gr, "newfolder", "New folder", true, false); drawButton(gr, "newbank", "New bank", true, false);
        drawButton(gr, "undo", "Undo", editor.hist.canUndo(), false); drawButton(gr, "redo", "Redo", editor.hist.canRedo(), false);
        if (status_.isNotEmpty())   // message line between the buttons and the search box
        {
            const auto r = Rect(at("redo").getRight() + 12.0f, at("redo").getY(), at("search").getX() - at("redo").getRight() - 24.0f, at("redo").getHeight());
            gr.setColour(juce::Colour(0xff9AA3B8)); gr.setFont(font(11.0f)); gr.drawText(status_, r, juce::Justification::centredLeft, true);
        }
        paintTree(gr); paintTable(gr);
        if (dropHint) { gr.setColour(juce::Colour(0x556F80AC)); gr.fillRoundedRectangle(at("table"), 4.0f); gr.setColour(juce::Colours::white); gr.setFont(font(15.0f)); gr.drawText("Drop .syx files to import", at("table"), juce::Justification::centred); }
    }

    // ----- mouse and keys -----
    void mouseMove(const juce::MouseEvent& e) override { hover = e.position; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { hover = { -1, -1 }; repaint(); }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const int step = w.deltaY > 0 ? -3 : 3;
        if (at("tree").contains(e.position)) treeTop = juce::jlimit(0, juce::jmax(0, (int) items.size() - treeRows()), treeTop + step);
        else if (at("table").contains(e.position)) rowTop = juce::jlimit(0, juce::jmax(0, (int) view.size() - tableRows()), rowTop + step);
        repaint();
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        grabKeyboardFocus(); const auto p = e.position; const bool right = e.mods.isPopupMenu();
        if (!right) { for (auto* k : { "prev", "next", "grab", "reset", "newfolder", "newbank", "undo", "redo" }) if (at(k).contains(p)) { toolbar(k); return; } }
        if (at("tree").contains(p)) { const int i = treeItemAt(p); if (i >= 0) { selectItem(i); if (right) treeMenu(i); } else if (right) treeMenu(-1); return; }
        if (at("table").contains(p))
        {
            const int v = viewRowAt(p);
            if (v >= 0) { selRow = v; if (!right && p.x >= colX(4) && starTarget(v)) { toggleStar(v); } }
            if (right) tableMenu(v); repaint();
        }
    }
    void mouseDoubleClick(const juce::MouseEvent& e) override { if (at("table").contains(e.position)) { const int v = viewRowAt(e.position); if (v >= 0 && e.position.x < colX(4)) renameRow(v); } }
    bool keyPressed(const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::upKey) { moveSelection(-1); return true; }
        if (k == juce::KeyPress::downKey) { moveSelection(1); return true; }
        if (k == juce::KeyPress::F2Key) { if (selRow >= 0) renameRow(selRow); return true; }
        if (k == juce::KeyPress::deleteKey) { if (selRow >= 0) removeRow(selRow); return true; }
        if (k == juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0)) { doUndo(); return true; }
        if (k == juce::KeyPress('y', juce::ModifierKeys::commandModifier, 0) || k == juce::KeyPress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)) { doRedo(); return true; }
        return false;
    }
    juce::String getTooltip() override
    {
        const auto p = hover;
        if (at("grab").contains(p)) return "Grab needs the message that asks the UltraNova for a patch. It is not in any document we have, so it waits for a capture of the Novation Librarian (see the roadmap). This page does not send MIDI to the device.";
        if (at("reset").contains(p)) return "Reset (Init) needs the bytes of an init patch, which we do not have. Waiting for the capture.";
        if (at("prev").contains(p) || at("next").contains(p)) return "Moves the selection in the list (ArrowUp and ArrowDown work too).";
        if (at("newfolder").contains(p)) return "Makes a folder inside the selected folder of your library.";
        if (at("newbank").contains(p)) return "Makes an empty bank (a .syx file of up to 128 patches) in the selected folder.";
        if (at("undo").contains(p) || at("redo").contains(p)) return "Undo and Redo for edits of the open bank (Ctrl+Z, Ctrl+Y). Every change is saved to the file at once.";
        if (at("search").contains(p)) return "Filters the open bank by name, category or genre.";
        if (at("tree").contains(p)) { const int i = treeItemAt(p); if (i >= 0 && items[(size_t) i].k == Item::Device) return "Reading the banks of the UltraNova needs the capture too. Not available yet."; if (i >= 0 && items[(size_t) i].k == Item::Header && items[(size_t) i].root) return jstr(root.u8string()) + "  (right-click: choose folder, backup, restore)"; }
        if (at("table").contains(p)) { const int v = viewRowAt(p); if (v >= 0 && p.x >= colX(4)) return "Click to mark or unmark as a favourite."; if (v >= 0) return "Double-click to rename. Right-click for category, genre, move, delete and export."; }
        return {};
    }

    // ----- files dropped from the desktop -----
    bool isInterestedInFileDrag(const juce::StringArray& files) override { for (auto& f : files) if (f.endsWithIgnoreCase(".syx")) return true; return false; }
    void fileDragEnter(const juce::StringArray&, int, int) override { dropHint = true; repaint(); }
    void fileDragExit(const juce::StringArray&) override { dropHint = false; repaint(); }
    void filesDropped(const juce::StringArray& files, int x, int y) override
    {
        dropHint = false; const bool onTable = at("table").contains(juce::Point<float>((float) x, (float) y)) && !openFile.empty();
        for (auto& f : files) if (f.endsWithIgnoreCase(".syx")) importFile(juce::File(f), onTable);
        repaint();
    }

private:
    struct Item { enum K { Header, Folder, Bank, Device } k = Header; int node = -1; juce::String text; int indent = 0; bool root = false; };

    Rect at(const char* k) const { auto it = g.find(k); return it == g.end() ? Rect() : it->second; }
    float rowH() const { return at("row").getHeight() > 4 ? at("row").getHeight() : 23.0f; }
    float treeRowH() const { return at("treeRow").getHeight() > 4 ? at("treeRow").getHeight() : 21.0f; }
    int tableRows() const { return juce::jmax(1, (int) ((at("table").getBottom() - at("row").getY() - 2.0f) / rowH())); }
    int treeRows() const { return juce::jmax(1, (int) ((at("tree").getBottom() - at("treeRow").getY() - 2.0f) / treeRowH())); }
    // column x positions, from the mockup grid: 44px, 1fr, 110px, 110px, 28px with 8px gaps inside 8px padding
    float colX(int c) const
    {
        const float x0 = at("table").getX() + 9.0f, inner = at("table").getWidth() - 18.0f, nameW = inner - 44 - 110 - 110 - 28 - 32;
        const float xs[5] = { x0, x0 + 52, x0 + 52 + nameW + 8, x0 + 52 + nameW + 8 + 118, x0 + 52 + nameW + 8 + 236 };
        return xs[juce::jlimit(0, 4, c)];
    }
    float colW(int c) const { const float inner = at("table").getWidth() - 18.0f, nameW = inner - 44 - 110 - 110 - 28 - 32; const float w[5] = { 44, nameW, 110, 110, 28 }; return w[juce::jlimit(0, 4, c)]; }

    void drawButton(juce::Graphics& gr, const char* k, const juce::String& text, bool enabled, bool green)
    {
        const auto r = at(k); if (r.isEmpty()) return; const bool over = enabled && r.contains(hover);
        juce::ColourGradient grad = green ? juce::ColourGradient(juce::Colour(0xff3FA45E), r.getX(), r.getY(), juce::Colour(0xff22663A), r.getX(), r.getBottom(), false)
                                          : juce::ColourGradient(juce::Colour(over ? 0xff20242C : 0xff15171C), r.getX(), r.getY(), juce::Colour(over ? 0xff20242C : 0xff15171C), r.getX(), r.getBottom(), false);
        gr.setGradientFill(grad); gr.fillRoundedRectangle(r, 4.0f); gr.setColour(juce::Colour(0xff444444)); gr.drawRoundedRectangle(r.reduced(0.5f), 4.0f, 1.0f);
        gr.setColour(juce::Colour(0xffEEEEEE).withAlpha(enabled ? 1.0f : 0.38f)); gr.setFont(font(11.0f)); gr.drawText(text, r, juce::Justification::centred, false);
    }
    static void folderIcon(juce::Graphics& gr, float x, float y, juce::Colour c) { gr.setColour(c); gr.fillRoundedRectangle(x, y + 2, 11, 8, 1.5f); gr.fillRoundedRectangle(x, y, 5, 4, 1.0f); }
    static void bankIcon(juce::Graphics& gr, float x, float y, juce::Colour c) { gr.setColour(c); for (int i = 0; i < 3; ++i) gr.fillRect(x, y + 1.0f + 3.0f * (float) i, 11.0f, 2.0f); }

    void paintTree(juce::Graphics& gr)
    {
        const auto t = at("tree"); if (t.isEmpty()) return; gr.saveState(); gr.reduceClipRegion(t.reduced(1).toNearestInt());
        float y = at("treeRow").getY(); const float h = treeRowH();
        for (int i = treeTop; i < (int) items.size() && y < t.getBottom(); ++i, y += h)
        {
            const auto& it = items[(size_t) i]; const Rect row(t.getX() + 5, y, t.getWidth() - 10, h);
            if (i == selItem) { gr.setColour(juce::Colour(0xff2E4D80)); gr.fillRect(row); }
            else if (it.k != Item::Header && row.contains(hover)) { gr.setColour(juce::Colour(0xff161B2A)); gr.fillRect(row); }
            const bool head = it.k == Item::Header, dim = it.k == Item::Device;
            const auto col = head ? juce::Colours::white : dim ? juce::Colour(0xff6C7385) : juce::Colour(0xffAEB5C2);
            const float x = row.getX() + 6.0f + (float) it.indent;
            if (it.k == Item::Folder) folderIcon(gr, x, y + 5, col.withAlpha(0.8f)); else if (it.k == Item::Bank) bankIcon(gr, x, y + 5, col.withAlpha(0.8f));
            gr.setColour(col); gr.setFont(font(12.0f));
            gr.drawText(it.text, juce::Rectangle<float>(x + (it.k == Item::Folder || it.k == Item::Bank ? 16.0f : 0.0f), y, row.getRight() - x - 18.0f, h), juce::Justification::centredLeft, true);
        }
        gr.restoreState();
    }

    void paintTable(juce::Graphics& gr)
    {
        const auto t = at("table"); if (t.isEmpty()) return; gr.saveState(); gr.reduceClipRegion(t.reduced(1).toNearestInt());
        if (openFile.empty() || editor.bank.empty() || view.empty())
        {
            gr.setColour(juce::Colour(0xff8C93A3)); gr.setFont(font(13.0f));
            const juce::String msg = openFile.empty() ? "Pick a bank on the left, or drop .syx files here to import them."
                                   : editor.bank.empty() ? "This bank is empty. Drop .syx files here, or right-click to import."
                                                         : "No patch matches the search.";
            gr.drawFittedText(msg, t.reduced(40).toNearestInt(), juce::Justification::centred, 3);
            gr.restoreState(); return;
        }
        float y = at("row").getY(); const float h = rowH();
        for (int v = rowTop; v < (int) view.size() && y < t.getBottom(); ++v, y += h)
        {
            const size_t bi = (size_t) view[(size_t) v]; const auto& e = editor.bank[bi]; const Rect row(t.getX() + 1, y, t.getWidth() - 2, h);
            gr.setColour(v == selRow ? juce::Colour(0xff2E4D80) : juce::Colour((v & 1) ? 0xff0E1220 : 0xff121729)); gr.fillRect(row);
            gr.setColour(juce::Colour(0xff1E2333)); gr.fillRect(row.getX(), row.getBottom() - 1.0f, row.getWidth(), 1.0f);
            gr.setFont(font(12.0f));
            gr.setColour(juce::Colour(0xff8C93A3)); gr.drawText(juce::String((int) bi).paddedLeft('0', 3), Rect(colX(0), y, colW(0), h), juce::Justification::centredLeft, false);
            gr.setColour(juce::Colour(0xffE8EAF0)); gr.drawText(jstr(lib::nameOf(e.raw)), Rect(colX(1), y, colW(1), h), juce::Justification::centredLeft, true);
            gr.setColour(juce::Colour(0xffAEB5C2));
            gr.drawText(jstr(e.meta.category), Rect(colX(2), y, colW(2), h), juce::Justification::centredLeft, true);
            gr.drawText(jstr(e.meta.genre), Rect(colX(3), y, colW(3), h), juce::Justification::centredLeft, true);
            juce::Path star; star.addStar({ colX(4) + 8.0f, y + h * 0.5f }, 5, 3.2f, 7.0f);
            if (e.meta.star) { gr.setColour(juce::Colour(0xffF2D36E)); gr.fillPath(star); } else { gr.setColour(juce::Colour(0xff3A4156)); gr.strokePath(star, juce::PathStrokeType(1.0f)); }
        }
        gr.restoreState();
    }

    // ----- model -----
    void status(const juce::String& s) { status_ = s; repaint(); }

    void reload()
    {
        std::error_code ec; lib::fs::create_directories(root / "My Patches", ec); lib::fs::create_directories(root / "Grabbed Patches", ec);
        nodes.clear(); lib::scan(root, 0, nodes);
        items.clear(); Item h; h.k = Item::Header; h.text = "On my computer"; h.root = true; items.push_back(h);
        for (size_t i = 0; i < nodes.size(); ++i) { Item it; it.k = nodes[i].isBank ? Item::Bank : Item::Folder; it.node = (int) i; it.text = jstr(nodes[i].name); it.indent = 12 + 14 * nodes[i].depth; items.push_back(it); }
        Item d; d.k = Item::Header; d.text = "On my UltraNova"; items.push_back(d);
        for (const char* b : { "Bank A", "Bank B", "Bank C", "Bank D" }) { Item it; it.k = Item::Device; it.text = b; it.indent = 12; items.push_back(it); }
        // keep the selection on the same path when the tree is rebuilt
        selItem = -1; if (!selPath.empty()) for (size_t i = 0; i < items.size(); ++i) if (items[i].node >= 0 && nodes[(size_t) items[i].node].path == selPath) selItem = (int) i;
        if (selItem < 0 && selPath.empty()) selItem = 0;
        if (selItem < 0) { selPath.clear(); selItem = 0; openFile.clear(); editor = {}; rebuildView(); }
        treeTop = juce::jlimit(0, juce::jmax(0, (int) items.size() - treeRows()), treeTop);
        repaint();
    }
    int treeItemAt(juce::Point<float> p) const
    {
        const float y0 = at("treeRow").getY(); if (p.y < y0) return -1; const int i = treeTop + (int) ((p.y - y0) / treeRowH()); return i >= 0 && i < (int) items.size() ? i : -1;
    }
    int viewRowAt(juce::Point<float> p) const
    {
        const float y0 = at("row").getY(); if (p.y < y0) return -1; const int v = rowTop + (int) ((p.y - y0) / rowH()); return v >= 0 && v < (int) view.size() ? v : -1;
    }
    bool starTarget(int v) const { return v >= 0 && v < (int) view.size(); }

    void selectItem(int i)
    {
        const auto& it = items[(size_t) i]; if (it.k == Item::Header && !it.root) return;   // the "On my UltraNova" title is not a place
        if (it.k == Item::Device) { status("The UltraNova's own banks can be read after the capture (see the roadmap)."); return; }
        selItem = i; selPath.clear(); openFile.clear(); editor = {}; selRow = -1; rowTop = 0;
        if (it.node >= 0) { selPath = nodes[(size_t) it.node].path; if (nodes[(size_t) it.node].isBank) openBank(selPath); }
        rebuildView(); repaint();
    }
    void openBank(const lib::fs::path& f)
    {
        auto l = lib::readBank(f);
        if (!l.ok) { status("Could not read " + jstr(f.filename().u8string())); return; }
        openFile = f; editor = {}; editor.bank = l.bank; selRow = l.bank.empty() ? -1 : 0;
        juce::String s = jstr(f.stem().u8string()) + ": " + juce::String((int) l.bank.size()) + " patches";
        if (l.info.rejected > 0) s += ", " + juce::String(l.info.rejected) + " skipped" + (l.info.reasons.empty() ? juce::String() : " (" + jstr(l.info.reasons.front()) + ")");
        status(s);
    }
    void rebuildView()
    {
        view.clear(); const auto q = search.getText().toLowerCase().trim();
        for (size_t i = 0; i < editor.bank.size(); ++i)
        {
            const auto& e = editor.bank[i];
            if (q.isNotEmpty() && !(jstr(lib::nameOf(e.raw)).toLowerCase().contains(q) || jstr(e.meta.category).toLowerCase().contains(q) || jstr(e.meta.genre).toLowerCase().contains(q))) continue;
            view.push_back((int) i);
        }
        if (selRow >= (int) view.size()) selRow = (int) view.size() - 1;
        rowTop = juce::jlimit(0, juce::jmax(0, (int) view.size() - tableRows()), rowTop);
    }
    void edited(const juce::String& msg)
    {
        if (!openFile.empty() && !lib::writeBank(openFile, editor.bank)) { status("Could not save " + jstr(openFile.filename().u8string()) + ". Is the folder writable?"); }
        else status(msg);
        rebuildView(); repaint();
    }
    int bankIndex(int v) const { return v >= 0 && v < (int) view.size() ? view[(size_t) v] : -1; }
    void moveSelection(int d)
    {
        if (view.empty()) return; const int base = selRow < 0 ? (d > 0 ? -1 : (int) view.size()) : selRow; selRow = juce::jlimit(0, (int) view.size() - 1, base + d);
        if (selRow < rowTop) rowTop = selRow; if (selRow >= rowTop + tableRows()) rowTop = selRow - tableRows() + 1; repaint();
    }
    void toggleStar(int v)
    {
        const int bi = bankIndex(v); if (bi < 0) return; auto m = editor.bank[(size_t) bi].meta; m.star = !m.star;
        if (editor.setMeta((size_t) bi, m)) edited(m.star ? "Marked as a favourite" : "Favourite removed");
    }
    void renameRow(int v)
    {
        const int bi = bankIndex(v); if (bi < 0) return; auto safe = juce::Component::SafePointer<LibraryView>(this);
        prompt("Rename patch", "Up to 16 characters (letters, digits and symbols; no accents).", jstr(lib::nameOf(editor.bank[(size_t) bi].raw)), [safe, bi](const juce::String& s)
        { if (safe && bi < (int) safe->editor.bank.size() && safe->editor.rename((size_t) bi, s.toStdString())) safe->edited("Renamed"); });
    }
    void removeRow(int v)
    {
        const int bi = bankIndex(v); if (bi < 0) return; if (editor.remove((size_t) bi)) { edited("Patch removed from the bank (Undo brings it back)"); selRow = juce::jmin(selRow, (int) view.size() - 1); }
    }
    void setMetaField(int v, bool category, const juce::String& val)
    {
        const int bi = bankIndex(v); if (bi < 0) return; auto m = editor.bank[(size_t) bi].meta; (category ? m.category : m.genre) = val.toStdString();
        if (editor.setMeta((size_t) bi, m)) edited(category ? "Category set" : "Genre set");
    }
    void doUndo() { if (editor.undo()) edited("Undone"); }
    void doRedo() { if (editor.redo()) edited("Redone"); }

    void toolbar(const juce::String& k)
    {
        if (k == "prev") moveSelection(-1); else if (k == "next") moveSelection(1);
        else if (k == "newfolder") newFolder(); else if (k == "newbank") newBank();
        else if (k == "undo") doUndo(); else if (k == "redo") doRedo();
        else if (k == "grab") status("Grab needs the device's SysEx request, which is not captured yet. Nothing was sent.");
        else if (k == "reset") status("Reset (Init) needs an init patch, which is not available yet. Nothing was sent.");
    }

    lib::fs::path targetFolder() const
    {
        if (selItem >= 0 && items[(size_t) selItem].node >= 0) { const auto& n = nodes[(size_t) items[(size_t) selItem].node]; return n.isBank ? n.path.parent_path() : n.path; }
        return root;
    }
    void newFolder()
    {
        auto safe = juce::Component::SafePointer<LibraryView>(this); const auto dir = targetFolder();
        prompt("New folder", "Name of the new folder:", "New folder", [safe, dir](const juce::String& s)
        { if (!safe) return; std::error_code ec; const auto p = lib::uniquePath(dir, lib::safeFileName(s.toStdString()), ""); lib::fs::create_directories(p, ec); safe->selPath = p; safe->reload(); safe->selectByPath(p); safe->status(ec ? "Could not create the folder" : "Folder created"); });
    }
    void newBank()
    {
        auto safe = juce::Component::SafePointer<LibraryView>(this); const auto dir = targetFolder();
        prompt("New bank", "Name of the new bank (a .syx file):", "New bank", [safe, dir](const juce::String& s)
        { if (!safe) return; const auto p = lib::uniquePath(dir, lib::safeFileName(s.toStdString()), ".syx"); const bool ok = lib::writeBank(p, {}); safe->reload(); if (ok) safe->selectByPath(p); safe->status(ok ? "Bank created" : "Could not create the bank"); });
    }
    void selectByPath(const lib::fs::path& p) { for (size_t i = 0; i < items.size(); ++i) if (items[i].node >= 0 && nodes[(size_t) items[i].node].path == p) { selectItem((int) i); return; } }

    // Reads a .syx file from anywhere. Into the open bank (dropped on the list) or as a new bank in the selected folder.
    void importFile(const juce::File& f, bool intoOpenBank)
    {
        juce::MemoryBlock mb; if (!f.loadFileAsData(mb)) { status("Could not read " + f.getFileName()); return; }
        auto imp = lib::importSysex(static_cast<const uint8_t*>(mb.getData()), mb.getSize());
        const juce::String skipped = imp.rejected ? ", " + juce::String(imp.rejected) + " skipped" + (imp.reasons.empty() ? juce::String() : " (" + jstr(imp.reasons.front()) + ")") : juce::String();
        if (imp.patches.empty()) { status("No UltraNova patches found in " + f.getFileName() + skipped); return; }
        if (intoOpenBank && !openFile.empty()) { const bool ok = editor.append(imp.patches); edited(ok ? "Added " + juce::String((int) imp.patches.size()) + " patches" + skipped : "The bank is full (128 patches)"); return; }
        const auto p = lib::uniquePath(targetFolder(), lib::safeFileName(f.getFileNameWithoutExtension().toStdString()), ".syx");
        lib::Bank b; for (auto& r : imp.patches) { if (b.size() >= lib::kMaxBank) break; b.push_back({ r, {} }); }
        const bool ok = lib::writeBank(p, b); reload(); if (ok) selectByPath(p);
        status(ok ? "Imported " + juce::String((int) b.size()) + " patches as " + jstr(p.stem().u8string()) + skipped : "Could not write the new bank");
    }

    void treeMenu(int i)
    {
        const bool onNode = i >= 0 && items[(size_t) i].node >= 0, isBank = onNode && nodes[(size_t) items[(size_t) i].node].isBank, isRoot = i >= 0 && items[(size_t) i].root, isDevice = i >= 0 && items[(size_t) i].k == Item::Device;
        if (isDevice) return;
        juce::PopupMenu m; m.addItem(1, "New folder"); m.addItem(2, "New bank"); m.addItem(3, "Import .syx file...");
        if (isBank) { m.addSeparator(); m.addItem(4, "Rename..."); m.addItem(5, "Export bank as .syx..."); m.addItem(6, "Move to trash..."); }
        else if (onNode) { m.addSeparator(); m.addItem(4, "Rename..."); m.addItem(6, "Move to trash..."); }
        m.addSeparator(); m.addItem(7, "Backup the whole library..."); m.addItem(8, "Restore from a backup..."); m.addItem(9, "Choose library folder..."); m.addItem(10, "Show the library folder");
        (void) isRoot;
        auto safe = juce::Component::SafePointer<LibraryView>(this);
        m.showMenuAsync(juce::PopupMenu::Options(), [safe, i](int r) { if (safe && r) safe->treeCommand(r, i); });
    }
    void treeCommand(int r, int i)
    {
        const bool onNode = i >= 0 && items[(size_t) i].node >= 0; const lib::Node* n = onNode ? &nodes[(size_t) items[(size_t) i].node] : nullptr;
        auto safe = juce::Component::SafePointer<LibraryView>(this);
        switch (r)
        {
            case 1: newFolder(); break; case 2: newBank(); break;
            case 3: chooser = std::make_unique<juce::FileChooser>("Import a .syx file", juce::File(), "*.syx");
                    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe](const juce::FileChooser& fc) { if (safe && fc.getResult().existsAsFile()) { safe->importFile(fc.getResult(), false); } }); break;
            case 4: if (n) { const auto path = n->path; const bool bank = n->isBank;
                        prompt("Rename", "New name:", jstr(bank ? path.stem().u8string() : path.filename().u8string()), [safe, path, bank](const juce::String& s)
                        { if (!safe) return; std::error_code ec; const auto to = path.parent_path() / (lib::safeFileName(s.toStdString()) + (bank ? ".syx" : ""));
                          if (lib::fs::exists(to, ec)) { safe->status("That name is already used"); return; }
                          lib::fs::rename(path, to, ec); if (!ec && bank && lib::fs::exists(lib::metaPath(path), ec)) lib::fs::rename(lib::metaPath(path), lib::metaPath(to), ec);
                          if (safe->selPath == path) safe->selPath = to; safe->reload(); safe->status(ec ? "Could not rename" : "Renamed"); }); } break;
            case 5: if (n && n->isBank) { const auto src = n->path; chooser = std::make_unique<juce::FileChooser>("Export bank as .syx", toFile(src).getSiblingFile(toFile(src).getFileName()), "*.syx");
                        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [safe, src](const juce::FileChooser& fc)
                        { if (!safe || fc.getResult() == juce::File()) return; auto l = lib::readBank(src); const auto bytes = lib::exportSysex(l.bank); auto out = fc.getResult().withFileExtension("syx");
                          safe->status(out.replaceWithData(bytes.data(), bytes.size()) ? "Exported " + juce::String((int) l.bank.size()) + " patches to " + out.getFileName() : "Could not write the file"); }); } break;
            case 6: if (n) { const auto path = n->path; confirm("Move to trash", "Move \"" + jstr(path.filename().u8string()) + "\" to the trash? You can restore it from there.", "Move to trash", [safe, path]
                        { if (!safe) return; const bool ok = toFile(path).moveToTrash(); if (ok && path.extension() == ".syx") toFile(lib::metaPath(path)).moveToTrash();
                          if (safe->selPath == path) { safe->selPath.clear(); } safe->reload(); safe->selectItem(0); safe->status(ok ? "Moved to the trash" : "Could not move it to the trash"); }); } break;
            case 7: chooser = std::make_unique<juce::FileChooser>("Choose where the backup folder goes", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory));
                    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [safe](const juce::FileChooser& fc)
                    { if (!safe || !fc.getResult().isDirectory()) return; std::string err; const auto d = lib::backup(safe->root, toPath(fc.getResult()), err);
                      safe->status(d.empty() ? "Backup failed: " + jstr(err) : "Backup saved in " + jstr(d.filename().u8string())); }); break;
            case 8: chooser = std::make_unique<juce::FileChooser>("Choose a backup folder (UltraNova-library-...)", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory));
                    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [safe](const juce::FileChooser& fc)
                    { if (!safe || !fc.getResult().isDirectory()) return; const auto r = lib::restore(toPath(fc.getResult()), safe->root); safe->reload();
                      safe->status(!r.err.empty() ? "Restore failed: " + jstr(r.err) : "Restored " + juce::String(r.copied) + " new, " + juce::String(r.renamed) + " added as (restored), " + juce::String(r.same) + " already there. Nothing was overwritten."); }); break;
            case 9: chooser = std::make_unique<juce::FileChooser>("Choose the library folder", toFile(root));
                    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [safe](const juce::FileChooser& fc)
                    { if (!safe || !fc.getResult().isDirectory()) return; safe->root = toPath(fc.getResult()); safe->selPath.clear(); safe->openFile.clear(); safe->editor = {}; safe->rebuildView(); safe->reload();
                      if (safe->onRootChanged) safe->onRootChanged(fc.getResult()); safe->status("Library: " + fc.getResult().getFullPathName()); }); break;
            case 10: toFile(root).revealToUser(); break;
            default: break;
        }
    }

    void tableMenu(int v)
    {
        juce::PopupMenu m; const bool has = v >= 0; const bool bankOpen = !openFile.empty();
        if (has)
        {
            const int bi = bankIndex(v); const auto meta = editor.bank[(size_t) bi].meta;
            m.addItem(1, "Rename..."); m.addItem(2, meta.star ? "Remove favourite" : "Mark as favourite");
            juce::PopupMenu cat; cat.addItem(100, "None", true, meta.category.empty()); for (size_t i = 0; i < lib::categories().size(); ++i) cat.addItem(101 + (int) i, lib::categories()[i], true, meta.category == lib::categories()[i]); cat.addSeparator(); cat.addItem(199, "Other...");
            juce::PopupMenu gen; gen.addItem(200, "None", true, meta.genre.empty()); for (size_t i = 0; i < lib::genres().size(); ++i) gen.addItem(201 + (int) i, lib::genres()[i], true, meta.genre == lib::genres()[i]); gen.addSeparator(); gen.addItem(299, "Other...");
            m.addSubMenu("Category", cat); m.addSubMenu("Genre", gen); m.addSeparator();
            m.addItem(3, "Move up", bi > 0); m.addItem(4, "Move down", bi + 1 < (int) editor.bank.size()); m.addItem(5, "Remove from the bank"); m.addItem(6, "Export this patch as .syx...");
        }
        if (bankOpen) { if (has) m.addSeparator(); m.addItem(7, "Import .syx file into this bank..."); m.addItem(8, "Export the bank as .syx..."); }
        if (m.getNumItems() == 0) return;
        auto safe = juce::Component::SafePointer<LibraryView>(this);
        m.showMenuAsync(juce::PopupMenu::Options(), [safe, v](int r) { if (safe && r) safe->tableCommand(r, v); });
    }
    void tableCommand(int r, int v)
    {
        auto safe = juce::Component::SafePointer<LibraryView>(this); const int bi = bankIndex(v);
        if (r == 1) renameRow(v);
        else if (r == 2) toggleStar(v);
        else if (r == 3 && bi >= 0) { if (editor.move((size_t) bi, -1)) { edited("Moved up"); selRow = juce::jmax(0, selRow - 1); } }
        else if (r == 4 && bi >= 0) { if (editor.move((size_t) bi, 1)) { edited("Moved down"); selRow = juce::jmin((int) view.size() - 1, selRow + 1); } }
        else if (r == 5) removeRow(v);
        else if (r == 100) setMetaField(v, true, {}); else if (r >= 101 && r <= 198) setMetaField(v, true, jstr(lib::categories()[(size_t) (r - 101)]));
        else if (r == 199) prompt("Category", "Your own category:", {}, [safe, v](const juce::String& s) { if (safe) safe->setMetaField(v, true, s); });
        else if (r == 200) setMetaField(v, false, {}); else if (r >= 201 && r <= 298) setMetaField(v, false, jstr(lib::genres()[(size_t) (r - 201)]));
        else if (r == 299) prompt("Genre", "Your own genre:", {}, [safe, v](const juce::String& s) { if (safe) safe->setMetaField(v, false, s); });
        else if (r == 6 && bi >= 0)
        {
            const auto bytes = lib::exportSysex(editor.bank, (size_t) bi, 1); const auto name = jstr(lib::safeFileName(lib::nameOf(editor.bank[(size_t) bi].raw)));
            chooser = std::make_unique<juce::FileChooser>("Export patch as .syx", toFile(openFile).getSiblingFile(name + ".syx"), "*.syx");
            chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [safe, bytes](const juce::FileChooser& fc)
            { if (!safe || fc.getResult() == juce::File()) return; auto out = fc.getResult().withFileExtension("syx"); safe->status(out.replaceWithData(bytes.data(), bytes.size()) ? "Exported to " + out.getFileName() : "Could not write the file"); });
        }
        else if (r == 7) { chooser = std::make_unique<juce::FileChooser>("Import a .syx file", juce::File(), "*.syx");
            chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe](const juce::FileChooser& fc) { if (safe && fc.getResult().existsAsFile()) safe->importFile(fc.getResult(), true); }); }
        else if (r == 8) { const auto bytes = lib::exportSysex(editor.bank); const auto name = jstr(openFile.stem().u8string());
            chooser = std::make_unique<juce::FileChooser>("Export bank as .syx", toFile(openFile).getSiblingFile(name + " export.syx"), "*.syx");
            chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [safe, bytes](const juce::FileChooser& fc)
            { if (!safe || fc.getResult() == juce::File()) return; auto out = fc.getResult().withFileExtension("syx"); safe->status(out.replaceWithData(bytes.data(), bytes.size()) ? "Exported to " + out.getFileName() : "Could not write the file"); }); }
    }

    // ----- small dialogs (async, so nothing blocks the host) -----
    void prompt(const juce::String& title, const juce::String& msg, const juce::String& initial, std::function<void(const juce::String&)> done)
    {
        auto* w = new juce::AlertWindow(title, msg, juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor("t", initial); w->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey)); w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        w->enterModalState(true, juce::ModalCallbackFunction::create([w, done](int r) { if (r == 1 && done) done(w->getTextEditorContents("t")); }), true);
    }
    void confirm(const juce::String& title, const juce::String& msg, const juce::String& okText, std::function<void()> done)
    {
        auto* w = new juce::AlertWindow(title, msg, juce::MessageBoxIconType::WarningIcon, this);
        w->addButton(okText, 1, juce::KeyPress(juce::KeyPress::returnKey)); w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        w->enterModalState(true, juce::ModalCallbackFunction::create([done](int r) { if (r == 1 && done) done(); }), true);
    }

    std::map<juce::String, Rect> g; std::function<juce::Font(float)> font; lib::fs::path root, selPath, openFile;
    std::vector<lib::Node> nodes; std::vector<Item> items; std::vector<int> view; lib::BankEditor editor;
    int selItem = 0, selRow = -1, treeTop = 0, rowTop = 0; bool dropHint = false; juce::Point<float> hover { -1, -1 }; juce::String status_;
    juce::TextEditor search; std::unique_ptr<juce::FileChooser> chooser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LibraryView)
};
}
