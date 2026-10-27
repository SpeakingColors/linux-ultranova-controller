// The real UltraNova Controller window, built from the approved mockup (docs/mockups).
// tools/export_mockup.py turns every mockup page into artwork (assets/ui/<page>.png, 2x) plus a list of where
// the live controls sit (assets/ui/<page>.json). This file draws the artwork and puts real controls on top.
// Controls that are not wired to a parameter yet are drawn disabled and listed in kKnobMap with an empty id.
#pragma once
#include <map>
#include <set>
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <map>
#include <memory>
#include <vector>
#include "BinaryData.h"
#include <catui/CatLookAndFeel.h>

namespace unova
{
constexpr int kW = 1348, kH = 760;          // design size, the same for every page
constexpr int kDefaultW = 1348;
constexpr int kCheek = 34;                  // wood cheek width in design pixels             // 100%

inline juce::MemoryBlock resource(const juce::String& original)
{
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        if (juce::String(BinaryData::originalFilenames[i]) == original)
        { int sz = 0; auto* d = BinaryData::getNamedResource(BinaryData::namedResourceList[i], sz); return juce::MemoryBlock(d, size_t(sz)); }
    return {};
}

// Which parameter each knob drives, in the order the knobs appear on the page (the order tools/export_mockup.py writes).
// An empty id means the knob has no parameter yet (the source document does not give one); it is drawn but disabled.
inline const std::map<juce::String, std::vector<juce::String>>& knobMap()
{
    static const std::map<juce::String, std::vector<juce::String>> m = {
        { "animate", { "", "", "", "", "", "", "", "" } },   // the 8 tweak knobs: the assigned targets live in patch data we do not have
        { "osc", { "cc_21", "cc_20", "cc_23", "cc_22", "cc_27", "cc_25", "cc_24",          // Oscillator 1: pulse width, wavetable interpolate, hardness, sync depth, cents, density detune, density
                   "cc_31", "cc_30", "cc_34", "cc_33", "cc_39", "cc_36", "cc_35",          // Oscillator 2
                   "cc_43", "cc_42", "cc_45", "cc_44", "cc_49", "cc_47", "cc_46",          // Oscillator 3
                   "cc_16", "cc_76", "cc_77",                                              // common: drift, vibrato rate, vibrato depth
                   "cc_58", "cc_59",                                                            // effects send (Pre-FX level, CC number unclear in the source), output (Post-FX level)
                   "cc_15", "cc_5",                                                        // unison detune, portamento time
                   "cc_108", "cc_111", "cc_114" } },                                       // amp envelope: velocity, repeat, level track
        { "filter", { "cc_61",
                      "cc_69", "cc_79", "cc_74", "cc_71", "cc_78", "cc_63",                // filter 1: keytrack, env amount, frequency, resonance, q normalise, drive
                      "cc_84", "cc_87", "cc_83", "cc_85", "cc_86", "cc_80",                // filter 2
                      "nrpn_0_0", "nrpn_0_7", "nrpn_0_10" } },                             // filter envelope: velocity, repeat, level track
        { "mod", { "nrpn_0_76", "nrpn_0_72", "nrpn_0_74",                                  // LFO 1: rate, slew, delay
                   "nrpn_0_56", "nrpn_0_63", "nrpn_0_66" } },                              // Envelope 6: delay, repeat, level track
        { "fx", { "nrpn_0_105", "nrpn_0_107", "nrpn_0_109", "nrpn_0_104", "nrpn_0_106", "nrpn_0_108",   // EQ levels and frequencies
                  "nrpn_0_98", "cc_8",                                                          // feedback, FX wet level (no clear CC)
                  "cc_91", "cc_92", "cc_93", "cc_94", "cc_95",                              // slot amounts
                  "nrpn_1_67", "nrpn_1_66", "nrpn_1_64", "nrpn_1_63",                       // arpeggiator velocity, pattern, gate time, rate sync
                  "nrpn_1_78",                                                              // chord transpose
                  "nrpn_1_56", "nrpn_1_57", "", "nrpn_1_59", "nrpn_1_60",                   // vocoder balance, width, level (unclear), shift, magnify
                  "cc_10", "cc_88", "cc_90" } },                                            // pan position, rate, depth
        { "global", {} }, { "lib", {} } };
    return m;
}

inline juce::String pageKeyFromHref(const juce::String& h)
{
    if (h == "UltraNova") return "animate"; if (h == "UNova_Osc") return "osc"; if (h == "UNova_Filter") return "filter"; if (h == "UNova_Mod") return "mod";
    if (h == "UNova_FX") return "fx"; if (h == "UNova_Global") return "global"; if (h == "UNova_Lib") return "lib"; return {};
}

// The mockup's fonts (Exo 2 and VT323, both OFL), loaded once from the bundled files.
inline juce::Typeface::Ptr face(int weight, bool vt)
{
    static juce::Typeface::Ptr f300, f400, f600, vt400;
    auto load = [](const char* name) { auto m = resource(name); return m.getSize() ? juce::Typeface::createSystemTypefaceFor(m.getData(), m.getSize()) : juce::Typeface::Ptr(); };
    if (!f400) { f300 = load("exo-2-latin-300-normal.ttf"); f400 = load("exo-2-latin-400-normal.ttf"); f600 = load("exo-2-latin-600-normal.ttf"); vt400 = load("vt323-latin-400-normal.ttf"); }
    if (vt) return vt400; return weight <= 300 ? f300 : weight >= 600 ? f600 : f400;
}
inline juce::Font makeFont(int weight, bool vt, float px) { return juce::Font(juce::FontOptions(face(weight, vt))).withHeight(px); }
inline float textWidth(const juce::Font& f, const juce::String& t) { juce::GlyphArrangement ga; ga.addLineOfText(f, t, 0, 0); return ga.getBoundingBox(0, -1, true).getWidth(); }

struct TextRun { juce::String t; juce::Rectangle<float> r; juce::Font font; juce::Colour col; int rot = 0; };
struct FilterDialSpec { juce::Rectangle<float> rect; juce::Colour ring, curve; };
struct IconSpec { int n = 1; juce::Rectangle<float> rect; juce::Colour color; };
struct DialSpec { juce::Rectangle<float> rect; juce::Colour color; };
struct KnobSpec { juce::Rectangle<float> rect; float angle = 0; };
struct NavSpec { juce::String page; juce::Rectangle<float> rect; };
struct ButtonSpec { juce::String text, label; juce::Rectangle<float> rect; };
struct MatrixRowSpec { int slot = 1; bool sel = false; juce::Colour bg; juce::Rectangle<float> c[5]; };
struct PageData
{
    juce::Image art; std::vector<KnobSpec> knobs; std::vector<NavSpec> nav; std::vector<ButtonSpec> buttons; std::vector<TextRun> runs; std::vector<DialSpec> dials; std::vector<IconSpec> icons; std::vector<juce::Rectangle<float>> navBars, faders, bars; juce::Rectangle<float> env; std::vector<FilterDialSpec> filterDials; std::vector<juce::Rectangle<float>> lfoWins, routeTiles; std::vector<MatrixRowSpec> matrixRows;
    static std::unique_ptr<PageData> load(const juce::String& key)
    {
        auto pd = std::make_unique<PageData>(); auto png = resource(key + ".png"); auto js = resource(key + ".json");
        pd->art = juce::ImageFileFormat::loadFrom(png.getData(), png.getSize());
        auto v = juce::JSON::parse(juce::String::fromUTF8((const char*) js.getData(), (int) js.getSize()));
        auto rectOf = [](const juce::var& r) { return juce::Rectangle<float>((float) (double) r["x"], (float) (double) r["y"], (float) (double) r["w"], (float) (double) r["h"]); };
        if (auto* ks = v["knobs"].getArray()) for (auto& k : *ks) { KnobSpec s; s.rect = rectOf(k["rect"]); s.angle = k["angle"].isVoid() ? -135.0f : (float) (double) k["angle"]; pd->knobs.push_back(s); }
        if (auto* ns = v["nav"].getArray()) for (auto& n : *ns) pd->nav.push_back({ pageKeyFromHref(n["page"].toString()), rectOf(n["rect"]) });
        if (auto* bs = v["buttons"].getArray()) for (auto& b : *bs) pd->buttons.push_back({ b["text"].toString(), b["label"].toString(), rectOf(b["rect"]) });
        if (auto* rs = v["runs"].getArray()) for (auto& r : *rs)
        {
            TextRun tr; tr.t = r["t"].toString(); tr.r = rectOf(r); tr.rot = (int) r["rot"]; const float size = (float) (double) r["size"], ls = (float) (double) r["ls"];
            auto c = r["col"]; tr.col = juce::Colour::fromFloatRGBA((float) (double) c[0] / 255.0f, (float) (double) c[1] / 255.0f, (float) (double) c[2] / 255.0f, (float) (double) c[3]);
            tr.font = makeFont((int) r["weight"], (bool) r["vt"], size); if (ls != 0.0f) tr.font = tr.font.withExtraKerningFactor(ls / size);
            const float len = (std::abs(tr.rot) % 180 != 0) ? tr.r.getHeight() : tr.r.getWidth(), w = textWidth(tr.font, tr.t);   // match the width the browser gave this text
            if (w > 1.0f && len > 1.0f) tr.font = tr.font.withHeight(size * juce::jlimit(0.8f, 1.25f, len / w));
            pd->runs.push_back(std::move(tr));
        }
        if (auto* ds = v["dials"].getArray()) for (auto& d : *ds) pd->dials.push_back({ rectOf(d), juce::Colour::fromString("ff" + d["color"].toString().fromFirstOccurrenceOf("#", false, false)) });
        if (auto* is = v["navIcons"].getArray()) for (auto& d : *is) pd->icons.push_back({ (int) d["n"], rectOf(d), juce::Colour::fromString("ff" + d["color"].toString().fromFirstOccurrenceOf("#", false, false)) });
        auto list = [&](const char* k, std::vector<juce::Rectangle<float>>& out) { if (auto* a = v[k].getArray()) for (auto& r : *a) out.push_back(rectOf(r)); };
        list("navBars", pd->navBars); list("faders", pd->faders); list("bars", pd->bars); if (v["env"].isObject()) pd->env = rectOf(v["env"]);
        auto col = [](const juce::String& c) { return juce::Colour::fromString("ff" + c.fromFirstOccurrenceOf("#", false, false)); };
        if (auto* fs = v["filterDials"].getArray()) for (auto& d : *fs) pd->filterDials.push_back({ rectOf(d), col(d["ring"].toString()), col(d["color"].toString()) });
        list("lfoWins", pd->lfoWins); list("routeTiles", pd->routeTiles);
        if (auto* mr = v["matrixRows"].getArray()) for (auto& r : *mr) { MatrixRowSpec m; m.slot = (int) r["slot"]; m.sel = (bool) r["sel"]; for (int i = 0; i < 5; ++i) m.c[i] = rectOf(r["c"][i]); pd->matrixRows.push_back(m); }
        return pd;
    }
};

// The metal knob of the mockup: ticks, a cyan value arc, a knurled ring, a domed cap and a pointer.
class KnobLook : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider& s) override
    {
        using juce::Colour; const float S = (float) juce::jmin(w, h); const float u = S / 68.0f;
        const juce::Point<float> c((float) x + (float) w / 2, (float) y + (float) h / 2); const bool on = s.isEnabled();
        const float a0 = juce::degreesToRadians(-135.0f), sweep = juce::degreesToRadians(270.0f), a = a0 + pos * sweep;
        auto pt = [&](float r, float ang) { return juce::Point<float>(c.x + std::sin(ang) * r, c.y - std::cos(ang) * r); };
        g.setColour(Colour(0xffAEB6C4).withAlpha(on ? 1.0f : 0.5f));
        for (int i = 0; i <= 10; ++i) { auto p = pt(32.0f * u, a0 + sweep * (float) i / 10.0f); g.fillEllipse(p.x - 1.1f * u, p.y - 1.1f * u, 2.2f * u, 2.2f * u); }
        if (pos > 0.002f)
        {
            juce::Path arc; arc.addCentredArc(c.x, c.y, 30.0f * u, 30.0f * u, 0.0f, a0, a, true);
            g.setColour(Colour(0xff7EE3E8).withAlpha(on ? 0.9f : 0.35f)); g.strokePath(arc, juce::PathStrokeType(2.0f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        const float R = 27.0f * u;                                         // knurled ring
        g.setColour(juce::Colours::black.withAlpha(0.45f)); g.fillEllipse(c.x - R, c.y - R + 4 * u, 2 * R, 2 * R);
        g.setColour(Colour(0xff060607)); g.fillEllipse(c.x - R - u, c.y - R - u, 2 * R + 2 * u, 2 * R + 2 * u);
        g.setColour(Colour(0xff8E949C)); g.fillEllipse(c.x - R, c.y - R, 2 * R, 2 * R);
        g.setColour(Colour(0xffC7CCD3));
        for (int i = 0; i < 45; ++i) { juce::Path wedge; const float t0 = juce::degreesToRadians(4.0f + 8.0f * (float) i), t1 = t0 + juce::degreesToRadians(4.0f);
            wedge.startNewSubPath(c); wedge.lineTo(pt(R, t0)); wedge.addCentredArc(c.x, c.y, R, R, 0.0f, t0, t1, false); wedge.closeSubPath(); g.fillPath(wedge); }
        const float D = 20.5f * u;                                         // domed cap
        juce::ColourGradient cap(Colour(0xffFFFFFF), c.x - D * 0.3f, c.y - D * 0.44f, Colour(0xff6E747C), c.x + D * 0.9f, c.y + D * 0.9f, true);
        cap.addColour(0.18, Colour(0xffE3E6EA)); cap.addColour(0.46, Colour(0xffB9BEC5)); cap.addColour(0.75, Colour(0xff8D939B));
        g.setGradientFill(cap); g.fillEllipse(c.x - D, c.y - D, 2 * D, 2 * D);
        g.setColour(juce::Colours::white.withAlpha(0.5f)); g.drawEllipse(c.x - D + 0.5f, c.y - D + 0.5f, 2 * D - 1, 2 * D - 1, 0.7f * u);
        g.setColour(Colour(0xff1C1E22)); g.drawLine(juce::Line<float>(pt(R - 7.6f * u, a), pt(R - 21.6f * u, a)), 2.0f * u);   // pointer
    }
};

// Waveform choices: 6 basic, 36 wavetables, audio input. The wavetable shapes are illustrative (the real tables are not in Novation's MIDI document).
inline int waveCount() { return 43; }
inline juce::String waveName(int i) { static const char* n[6] = { "Sine", "Triangle", "Sawtooth", "Saw 9:1 PW", "Square", "Pulse" }; return i < 6 ? n[i] : i < 42 ? "Wavetable " + juce::String(i - 5) : "Audio in"; }
inline juce::String waveShort(int i) { static const char* n[6] = { "Sine", "Triangle", "Sawtooth", "Saw 9:1 PW", "Square", "Pulse" }; return i < 6 ? n[i] : i < 42 ? "WT " + juce::String(i - 5) : "Audio"; }
inline std::function<float(float)> waveFn(int i)
{
    const float tp = juce::MathConstants<float>::twoPi;
    switch (i)
    {
        case 0: return [=](float t) { return std::sin(tp * t); };
        case 1: return [](float t) { return t < 0.5f ? 4 * t - 1 : 3 - 4 * t; };
        case 2: return [](float t) { return 2 * t - 1; };
        case 3: return [](float t) { return t < 0.9f ? 2 * (t / 0.9f) - 1 : -1.0f; };
        case 4: return [](float t) { return t < 0.5f ? 1.0f : -1.0f; };
        case 5: return [](float t) { return t < 0.25f ? 1.0f : -1.0f; };
    }
    if (i >= 42) return [tp](float t) { return 0.75f * std::sin(tp * 2 * t) + 0.25f * std::sin(tp * 7 * t + 1); };
    float x = (float) (i * 7 + 3); auto rnd = [&x] { x = std::fmod(x * 9301.0f + 49297.0f, 233280.0f); return x / 233280.0f; };
    std::array<std::pair<float, float>, 5> h; for (int k = 0; k < 5; ++k) { const float a = rnd() / (float) (k + 1) * (1.6f - (float) (k + 1) * 0.2f), ph = rnd() * 6.28f; h[(size_t) k] = { a, ph }; }
    auto eval = [h, tp](float t) { float v = 0; for (size_t k = 0; k < 5; ++k) v += h[k].first * std::sin(tp * (float) (k + 1) * t + h[k].second); return v; };
    float m = 0.0001f; for (int q = 0; q < 64; ++q) m = juce::jmax(m, std::abs(eval((float) q / 64.0f)));
    return [=](float t) { return eval(t) / m; };
}
// amount 0..1 is the Pulsewidth/WT index control: it sets the duty of the Pulse wave and morphs a wavetable toward the next one (illustrative, like the tables).
inline juce::Path wavePath(int i, juce::Rectangle<float> r, float amount = -1.0f)
{
    juce::Path p; auto f = waveFn(i); auto f2 = (amount >= 0.0f && i >= 6 && i < 41) ? waveFn(i + 1) : f;
    const float duty = 0.05f + 0.9f * juce::jlimit(0.0f, 1.0f, amount < 0.0f ? 0.25f : amount);
    for (int k = 0; k <= 48; ++k)
    {
        const float t = (float) k / 48.0f, x = r.getX() + t * r.getWidth();
        float v = f(t); if (amount >= 0.0f) { if (i == 5) v = t < duty ? 1.0f : -1.0f; else if (i >= 6 && i < 41) v += (f2(t) - v) * amount; }
        const float y = r.getCentreY() - v * r.getHeight() / 2; if (k) p.lineTo(x, y); else p.startNewSubPath(x, y);
    }
    return p;
}

// The round waveform window of one oscillator. Click it to open the picker.
class WaveDial : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void(WaveDial&)> onOpen;
    WaveDial(juce::RangedAudioParameter* p, juce::Colour c) : param(p), colour(c)
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor); setTooltip("Click to choose a waveform (value meanings are assumed, not yet checked on a real UltraNova)");
        if (p) { attach = std::make_unique<juce::ParameterAttachment>(*p, [this](float v) { index = juce::jlimit(0, waveCount() - 1, juce::roundToInt(v)); repaint(); }); attach->sendInitialUpdate(); }
    }
    void followShape(juce::RangedAudioParameter* q)
    {
        if (!q) return; shapeAttach = std::make_unique<juce::ParameterAttachment>(*q, [this, q](float v) { amount = juce::jlimit(0.0f, 1.0f, q->convertTo0to1(v)); repaint(); }); shapeAttach->sendInitialUpdate();
    }
    void choose(int i) { index = i; if (attach) attach->setValueAsCompleteGesture((float) i); repaint(); }
    int current() const { return index; }
    juce::Colour colour;
    void paint(juce::Graphics& g) override
    {
        const float u = (float) juce::jmin(getWidth(), getHeight()) / 64.0f; const juce::Point<float> c((float) getWidth() / 2, (float) getHeight() / 2);
        auto at = [&](float x, float y) { return juce::Point<float>(c.x + (x - 32) * u, c.y + (y - 32) * u); };
        g.setColour(juce::Colours::black); g.fillEllipse(c.x - 30 * u, c.y - 30 * u, 60 * u, 60 * u);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff0A0A0B), c.x, c.y, colour.withAlpha(0.55f), c.x + 28 * u, c.y, true));
        auto cg = juce::ColourGradient(juce::Colour(0xff0A0A0B), c.x, c.y, colour.withAlpha(0.55f), c.x + 28 * u, c.y, true); cg.addColour(0.6, juce::Colour(0xff0A0A0B)); g.setGradientFill(cg);
        g.fillEllipse(c.x - 28 * u, c.y - 28 * u, 56 * u, 56 * u); g.setColour(colour); g.drawEllipse(c.x - 28 * u, c.y - 28 * u, 56 * u, 56 * u, 1.2f * u);
        g.setColour(juce::Colour(0xff0B0C0E)); g.fillEllipse(c.x - 18 * u, c.y - 18 * u, 36 * u, 36 * u); g.setColour(juce::Colour(0xff3A3D44)); g.drawEllipse(c.x - 18 * u, c.y - 18 * u, 36 * u, 36 * u, u);
        g.setColour(colour); g.strokePath(wavePath(index, { at(14, 22).x, at(14, 22).y, 36 * u, 22 * u }, amount), juce::PathStrokeType(2.4f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setFont(makeFont(400, false, 5.0f * u)); g.drawText(waveName(index).toLowerCase(), juce::Rectangle<float>(c.x - 26 * u, c.y + 20 * u, 52 * u, 8 * u), juce::Justification::centred, false);
    }
    void mouseDown(const juce::MouseEvent&) override { if (onOpen) onOpen(*this); }
private:
    juce::RangedAudioParameter* param; std::unique_ptr<juce::ParameterAttachment> attach, shapeAttach; int index = 2; float amount = -1.0f;
};

// The pop-up grid: one tile per waveform with its number, name and shape.
class WavePicker : public juce::Component
{
public:
    std::function<void(int)> onPick; std::function<void()> onClose;
    WavePicker(int cur, juce::Colour c) : current(cur), colour(c) { setSize(kCols * (kTW + kGap) - kGap + 2 * kPad, 34 + ((waveCount() + kCols - 1) / kCols) * (kTH + kGap) - kGap + kPad); }
    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillRoundedRectangle(getLocalBounds().toFloat().translated(0, 6), 10);
        g.setColour(juce::Colour(0xff15171C)); g.fillRoundedRectangle(getLocalBounds().toFloat(), 10); g.setColour(juce::Colour(0xff3A3F4A)); g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 10, 1);
        g.setColour(juce::Colour(0xffE6E8EC)); g.setFont(makeFont(600, false, 14)); g.drawText("Waveform", kPad, 8, 200, 20, juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff8D93A0)); g.setFont(makeFont(400, false, 10)); g.drawText("6 basic, 36 wavetables, audio input. Wavetable shapes are illustrative.", 120, 8, getWidth() - 120 - kPad, 20, juce::Justification::centredRight);
        for (int i = 0; i < waveCount(); ++i)
        {
            auto r = tile(i).toFloat(); const bool on = i == current, hot = i == hover;
            g.setColour(on ? juce::Colour(0xff222833) : hot ? juce::Colour(0xff252A33) : juce::Colour(0xff1B1E25)); g.fillRoundedRectangle(r, 6);
            g.setColour(on ? colour : juce::Colour(0xff2E333D)); g.drawRoundedRectangle(r.reduced(0.5f), 6, 1);
            g.setColour(juce::Colour(0xff8D93A0)); g.setFont(makeFont(400, false, 9)); g.drawText(juce::String(i + 1), r.reduced(6, 4).withHeight(12), juce::Justification::topLeft);
            g.setColour(on ? colour : juce::Colour(0xff9AA3B5)); g.strokePath(wavePath(i, r.reduced(10, 0).withTrimmedTop(16).withHeight(24)), juce::PathStrokeType(1.6f));
            g.setColour(on ? juce::Colours::white : juce::Colour(0xffC5CAD6)); g.setFont(makeFont(400, false, 10)); g.drawText(waveShort(i), r.withTrimmedTop(r.getHeight() - 16).withHeight(14), juce::Justification::centred);
        }
    }
    void mouseMove(const juce::MouseEvent& e) override { const int h = hit(e.getPosition()); if (h != hover) { hover = h; repaint(); } }
    void mouseDown(const juce::MouseEvent& e) override { const int h = hit(e.getPosition()); if (h >= 0 && onPick) onPick(h); }
    int current, hover = -1; juce::Colour colour;
private:
    static constexpr int kCols = 8, kTW = 80, kTH = 64, kGap = 6, kPad = 10;
    juce::Rectangle<int> tile(int i) const { return { kPad + (i % kCols) * (kTW + kGap), 34 + (i / kCols) * (kTH + kGap), kTW, kTH }; }
    int hit(juce::Point<int> p) const { for (int i = 0; i < waveCount(); ++i) if (tile(i).contains(p)) return i; return -1; }
};

// A value box of the mockup made live: shows one controller value, drag up/down or use the wheel to change it, double click resets.
// The maker's document gives no ranges, so it shows the plain controller number (centred ones as minus 64 to plus 63).
class Field : public juce::Component, public juce::SettableTooltipClient
{
public:
    Field(juce::RangedAudioParameter* p, bool centred, const TextRun& run, juce::Rectangle<float> box) : param(p), centre(centred), font(run.font), col(run.col)
    {
        setBounds(box.toNearestInt()); setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        setTooltip(p ? p->getName(60) + ": drag up or down, wheel, double click resets. Shows the plain controller value (range not in the maker's document)" : "");
    }
    static juce::String text(juce::RangedAudioParameter* p, bool centred)
    {
        if (!p) return "-"; const int v = juce::roundToInt(p->convertFrom0to1(p->getValue()));
        return centred ? (v - 64 >= 0 ? "+" : "") + juce::String(v - 64) : juce::String(v);
    }
    void paint(juce::Graphics& g) override { last = text(param, centre); g.setColour(col); g.setFont(font); g.drawText(last, getLocalBounds().toFloat(), juce::Justification::centred, false); }
    void refresh() { if (text(param, centre) != last) repaint(); }
    void mouseDown(const juce::MouseEvent&) override { if (param) { start = param->convertFrom0to1(param->getValue()); param->beginChangeGesture(); } }
    void mouseDrag(const juce::MouseEvent& e) override { set(start - (float) e.getDistanceFromDragStartY() / 2.0f); }
    void mouseUp(const juce::MouseEvent&) override { if (param) param->endChangeGesture(); }
    void mouseDoubleClick(const juce::MouseEvent&) override { if (param) { param->beginChangeGesture(); set(centre ? 64.0f : 0.0f); param->endChangeGesture(); } }
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    { if (param) { param->beginChangeGesture(); set(param->convertFrom0to1(param->getValue()) + (w.deltaY > 0 ? 1.0f : -1.0f)); param->endChangeGesture(); } }
private:
    void set(float v) { if (!param) return; param->setValueNotifyingHost(param->convertTo0to1(juce::jlimit(0.0f, 127.0f, std::round(v)))); repaint(); }
    juce::RangedAudioParameter* param; bool centre; juce::Font font; juce::Colour col; float start = 0; juce::String last;
};

inline float pget(juce::RangedAudioParameter* p) { return p ? p->convertFrom0to1(p->getValue()) : 0.0f; }
inline void pset(juce::RangedAudioParameter* p, float v) { if (p) p->setValueNotifyingHost(p->convertTo0to1(juce::jlimit(0.0f, 127.0f, std::round(v)))); }

// A mixer fader of the Oscillators page: drag it or use the wheel; double click sets 0. Shows the plain controller value in its tooltip.
class Fader : public juce::Component, public juce::SettableTooltipClient
{
public:
    Fader(juce::RangedAudioParameter* p, const juce::String& name, juce::Rectangle<float> box) : param(p), title(name), track(box)
    { setBounds(box.expanded(9, 3).toNearestInt()); setMouseCursor(juce::MouseCursor::UpDownResizeCursor); tip(); }
    void refresh() { if (pget(param) != last) { repaint(); tip(); } }
    void paint(juce::Graphics& g) override
    {
        last = pget(param); const auto r = track.translated(-(float) getX(), -(float) getY());
        g.setColour(juce::Colour(0xff222222)); g.fillRect(r); g.setColour(juce::Colour(0xff444444)); g.drawRect(r, 1.0f);
        const auto in = r.reduced(1.0f); const float h = juce::jmax(2.0f, in.getHeight() * last / 127.0f); const auto f = juce::Rectangle<float>(in.getX(), in.getBottom() - h, in.getWidth(), h);
        if (last <= 0.0f) { g.setColour(juce::Colour(0xff333333)); g.fillRect(f); }
        else { g.setGradientFill(juce::ColourGradient(juce::Colour(0xff4BE38A), 0, in.getBottom(), juce::Colour(0xffC8FFD9), 0, in.getY(), false)); g.fillRect(f); }
    }
    void mouseDown(const juce::MouseEvent& e) override { if (param) { param->beginChangeGesture(); at(e); } }
    void mouseDrag(const juce::MouseEvent& e) override { at(e); }
    void mouseUp(const juce::MouseEvent&) override { if (param) param->endChangeGesture(); }
    void mouseDoubleClick(const juce::MouseEvent&) override { if (param) { param->beginChangeGesture(); pset(param, 0); param->endChangeGesture(); } }
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) override { if (param) { param->beginChangeGesture(); pset(param, pget(param) + (w.deltaY > 0 ? 2.0f : -2.0f)); param->endChangeGesture(); } }
private:
    void at(const juce::MouseEvent& e) { const auto t = track.translated(-(float) getX(), -(float) getY()).reduced(1.0f); pset(param, 127.0f * (t.getBottom() - e.position.y) / t.getHeight()); }
    void tip() { setTooltip(title + ": " + juce::String(juce::roundToInt(pget(param))) + " (drag, wheel, double click sets 0; plain controller value)"); }
    juce::RangedAudioParameter* param; juce::String title; juce::Rectangle<float> track; float last = -1;
};

// A thin horizontal slider of the envelope panel (the bars next to the graph).
class Bar : public juce::Component, public juce::SettableTooltipClient
{
public:
    Bar(juce::RangedAudioParameter* p, const juce::String& name, juce::Rectangle<float> box) : param(p), title(name), track(box)
    { setBounds(box.expanded(0, 5).toNearestInt()); setMouseCursor(juce::MouseCursor::LeftRightResizeCursor); tip(); }
    void refresh() { if (pget(param) != last) { repaint(); tip(); } }
    void paint(juce::Graphics& g) override
    {
        last = pget(param); const auto r = track.translated(-(float) getX(), -(float) getY());
        g.setColour(juce::Colour(0xff2C3B52)); g.fillRect(r); g.setColour(juce::Colour(0xffE6E8EC)); g.fillRect(r.withWidth(r.getWidth() * last / 127.0f));
    }
    void mouseDown(const juce::MouseEvent& e) override { if (param) { param->beginChangeGesture(); at(e); } }
    void mouseDrag(const juce::MouseEvent& e) override { at(e); }
    void mouseUp(const juce::MouseEvent&) override { if (param) param->endChangeGesture(); }
    void mouseDoubleClick(const juce::MouseEvent&) override { if (param) { param->beginChangeGesture(); pset(param, param->convertFrom0to1(param->getDefaultValue())); param->endChangeGesture(); } }
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) override { if (param) { param->beginChangeGesture(); pset(param, pget(param) + (w.deltaY > 0 ? 2.0f : -2.0f)); param->endChangeGesture(); } }
private:
    void at(const juce::MouseEvent& e) { pset(param, 127.0f * (e.position.x - (track.getX() - (float) getX())) / track.getWidth()); }
    void tip() { setTooltip(title + ": " + juce::String(juce::roundToInt(pget(param))) + " (drag, wheel, double click resets; plain controller value)"); }
    juce::RangedAudioParameter* param; juce::String title; juce::Rectangle<float> track; float last = -1;
};

// The amp envelope graph. Attack, decay, sustain level, sustain time and release come from the controllers; drag the four dots to change them.
// Segment lengths are drawn illustratively (the maker's document gives no time scale).
class EnvGraph : public juce::Component, public juce::SettableTooltipClient
{
public:
    EnvGraph(juce::RangedAudioParameter* a, juce::RangedAudioParameter* d, juce::RangedAudioParameter* s, juce::RangedAudioParameter* st, juce::RangedAudioParameter* r, juce::Rectangle<float> box)
        : A(a), D(d), S(s), ST(st), R(r) { setBounds(box.toNearestInt()); }
    void refresh() { const auto sig = sig5(); if (sig != last) { last = sig; repaint(); } }
    void paint(juce::Graphics& g) override
    {
        last = sig5(); const auto b = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(juce::Colour(0xff0A1626)); g.fillRoundedRectangle(b, 6); g.setColour(juce::Colour(0xff4E5F86)); g.drawRoundedRectangle(b, 6, 1);
        const auto p = pts(); juce::Path path; path.startNewSubPath(p[0]); for (int i = 1; i < 5; ++i) path.lineTo(p[(size_t) i]);
        const float dash[2] = { 3, 3 }; g.setColour(juce::Colour(0xff2C3B52));
        for (int i = 1; i <= 3; ++i) { juce::Line<float> l(p[(size_t) i].x, 6, p[(size_t) i].x, (float) getHeight() - 6); juce::Path dl; juce::PathStrokeType(1).createDashedStroke(dl, juce::Path(), dash, 2); g.drawDashedLine(l, dash, 2, 1.0f); }
        g.setColour(juce::Colour(0xff7EE3E8)); g.strokePath(path, juce::PathStrokeType(2.0f));
        for (int i = 1; i <= 4; ++i) { g.setColour(juce::Colour(0xff0D1B2E)); g.fillEllipse(p[(size_t) i].x - 3.5f, p[(size_t) i].y - 3.5f, 7, 7); g.setColour(juce::Colour(0xff7EE3E8)); g.drawEllipse(p[(size_t) i].x - 3.5f, p[(size_t) i].y - 3.5f, 7, 7, 1.6f); }
    }
    void mouseMove(const juce::MouseEvent& e) override
    {
        const int h = hit(e.position); setMouseCursor(h > 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        static const char* nm[5] = { "", "Attack time, drag left or right", "Decay time and sustain level, drag", "Sustain time, drag left or right", "Release time, drag left or right" };
        setTooltip(h > 0 ? juce::String(nm[h]) + " (plain controller values, time scale illustrative)" : "Amp envelope");
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        grab = hit(e.position); if (grab < 1) return; start = e.position; v0 = { pget(A), pget(D), pget(S), pget(ST), pget(R) }; k0 = scale();
        for (auto* q : { A, D, S, ST, R }) if (q) q->beginChangeGesture();
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (grab < 1) return; const float dx = e.position.x - start.x, dy = e.position.y - start.y; const float perX = 127.0f / (70.0f * k0), perXs = 127.0f / (50.0f * k0);
        if (grab == 1) pset(A, v0[0] + dx * perX);
        else if (grab == 2) { pset(D, v0[1] + dx * perX); pset(S, v0[2] - dy * 127.0f / (float) (getHeight() - 26)); }
        else if (grab == 3) pset(ST, v0[3] + dx * perXs);
        else pset(R, v0[4] + dx * perX);
    }
    void mouseUp(const juce::MouseEvent&) override { if (grab > 0) for (auto* q : { A, D, S, ST, R }) if (q) q->endChangeGesture(); grab = 0; }
private:
    float scale() const
    {
        const float wa = 24 + pget(A) / 127 * 70, wd = 24 + pget(D) / 127 * 70, ws = 24 + pget(ST) / 127 * 50, wr = 24 + pget(R) / 127 * 70;
        return juce::jmin(1.0f, ((float) getWidth() - 30.0f) / (wa + wd + ws + wr));
    }
    std::array<juce::Point<float>, 5> pts() const
    {
        const float k = scale(), left = 10, top = 10, bottom = (float) getHeight() - 16, yS = bottom - pget(S) / 127 * (bottom - top);
        const float x1 = left + (24 + pget(A) / 127 * 70) * k, x2 = x1 + (24 + pget(D) / 127 * 70) * k, x3 = x2 + (24 + pget(ST) / 127 * 50) * k, x4 = x3 + (24 + pget(R) / 127 * 70) * k;
        return { juce::Point<float>(left, bottom), { x1, top }, { x2, yS }, { x3, yS }, { x4, bottom } };
    }
    int hit(juce::Point<float> m) const { const auto p = pts(); for (int i = 4; i >= 1; --i) if (p[(size_t) i].getDistanceFrom(m) < 9.0f) return i; return 0; }
    float sig5() const { return pget(A) + pget(D) * 131 + pget(S) * 17011 + pget(ST) * 997 + pget(R) * 10007; }
    juce::RangedAudioParameter *A, *D, *S, *ST, *R; float last = -1, k0 = 1; int grab = 0; juce::Point<float> start; std::array<float, 5> v0{};
};

// Filter types in the order of the maker's list ("14 types"). Which controller value picks which type is not in the document: assumed value 0..13 in this order.
struct FType { const char* name; int kind, a, b; bool noRes; };   // kind 0 low pass, 1 band pass (a high pass side, b low pass side), 2 high pass
inline const FType& filterType(int i)
{
    static const FType t[14] = { { "LP6NoRes", 0, 1, 0, true }, { "LP12", 0, 2, 0, false }, { "LP18", 0, 3, 0, false }, { "LP24", 0, 4, 0, false }, { "BP6/6", 1, 1, 1, false }, { "BP12/12", 1, 2, 2, false }, { "BP6/12", 1, 1, 2, false },
        { "BP12/6", 1, 2, 1, false }, { "BP6/18", 1, 1, 3, false }, { "BP18/6", 1, 3, 1, false }, { "HP6NoRes", 2, 1, 0, true }, { "HP12", 2, 2, 0, false }, { "HP18", 2, 3, 0, false }, { "HP24", 2, 4, 0, false } };
    return t[juce::jlimit(0, 13, i)];
}
// Illustrative response: cutoff 20 Hz to 20 kHz over the controller range (assumed), resonance bump up to 18 dB. Returns dB at frequency f.
inline float filterDb(const FType& ft, float freqV, float resV, float f)
{
    const float fc = 20.0f * std::pow(1000.0f, freqV / 127.0f); auto lp = [](float x, int n) { return 1.0f / std::sqrt(1.0f + std::pow(x, 2.0f * (float) n)); };
    float m;
    if (ft.kind == 0) m = lp(f / fc, ft.a); else if (ft.kind == 2) m = lp(fc / f, ft.a); else m = lp(f / (fc * 1.6f), ft.b) * lp((fc / 1.6f) / f, ft.a);
    float db = 20.0f * std::log10(juce::jmax(m, 1.0e-4f));
    if (!ft.noRes) { const float l = std::log(f / fc); db += resV / 127.0f * 18.0f * std::exp(-l * l / (2.0f * 0.14f * 0.14f)); }
    return db;
}
// The round filter window: draws the response of the current type, frequency and resonance. Drag sideways for frequency, up or down for resonance.
class FilterDial : public juce::Component, public juce::SettableTooltipClient
{
public:
    FilterDial(juce::RangedAudioParameter* t, juce::RangedAudioParameter* f, juce::RangedAudioParameter* r, juce::Colour ringC, juce::Colour curveC, juce::Rectangle<float> box, const juce::String& name)
        : T(t), F(f), R(r), ring(ringC), curve(curveC), title(name) { setBounds(box.toNearestInt()); setMouseCursor(juce::MouseCursor::UpDownLeftRightResizeCursor); tip(); }
    void refresh() { const float sg = pget(T) + pget(F) * 31 + pget(R) * 1009; if (sg != last) { repaint(); tip(); } }
    void paint(juce::Graphics& g) override
    {
        last = pget(T) + pget(F) * 31 + pget(R) * 1009; const float u = (float) juce::jmin(getWidth(), getHeight()) / 64.0f; const juce::Point<float> c((float) getWidth() / 2, (float) getHeight() / 2);
        g.setColour(juce::Colours::black); g.fillEllipse(c.x - 30 * u, c.y - 30 * u, 60 * u, 60 * u);
        g.setColour(juce::Colour(0xff0B0C0E)); g.fillEllipse(c.x - 28 * u, c.y - 28 * u, 56 * u, 56 * u); g.setColour(ring); g.drawEllipse(c.x - 28 * u, c.y - 28 * u, 56 * u, 56 * u, 1.2f * u);
        const auto& ft = filterType(juce::roundToInt(pget(T))); juce::Path p; const float x0 = c.x - 20 * u, w = 40 * u;
        for (int i = 0; i <= 64; ++i)
        {
            const float t = (float) i / 64.0f, db = filterDb(ft, pget(F), pget(R), 20.0f * std::pow(1000.0f, t)), y = c.y + (14.0f - juce::jlimit(-14.0f, 14.0f, db) / 50.0f * 34.0f - 8.0f) * u - 6 * u;
            if (i) p.lineTo(x0 + t * w, y); else p.startNewSubPath(x0 + t * w, y);
        }
        g.saveState(); g.reduceClipRegion(juce::Rectangle<float>(c.x - 24 * u, c.y - 24 * u, 48 * u, 38 * u).toNearestInt()); g.setColour(curve); g.strokePath(p, juce::PathStrokeType(2.0f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); g.restoreState();
        g.setColour(juce::Colours::white); g.setFont(makeFont(400, false, 7.0f * u)); g.drawText(ft.name, juce::Rectangle<float>(c.x - 26 * u, c.y + 12 * u, 52 * u, 10 * u), juce::Justification::centred, false);
    }
    void mouseDown(const juce::MouseEvent&) override { f0 = pget(F); r0 = pget(R); for (auto* q : { F, R }) if (q) q->beginChangeGesture(); }
    void mouseDrag(const juce::MouseEvent& e) override { pset(F, f0 + (float) e.getOffsetFromDragStart().x * 0.9f); pset(R, r0 - (float) e.getOffsetFromDragStart().y * 0.9f); }
    void mouseUp(const juce::MouseEvent& e) override
    {
        for (auto* q : { F, R }) if (q) q->endChangeGesture();
        if (e.getDistanceFromDragStart() < 4 && T)   // a click without dragging: choose the filter type
        {
            juce::PopupMenu m; const int cur = juce::roundToInt(pget(T)); for (int i = 0; i < 14; ++i) m.addItem(i + 1, filterType(i).name, true, i == cur);
            m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this), [this, safe = juce::Component::SafePointer<FilterDial>(this)](int r)
                { if (safe != nullptr && r > 0 && T) { T->beginChangeGesture(); pset(T, (float) (r - 1)); T->endChangeGesture(); repaint(); } });
        }
    }
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) override { if (F) { F->beginChangeGesture(); pset(F, pget(F) + (w.deltaY > 0 ? 2.0f : -2.0f)); F->endChangeGesture(); } }
private:
    void tip() { setTooltip(title + ": " + filterType(juce::roundToInt(pget(T))).name + ", frequency " + juce::String(juce::roundToInt(pget(F))) + ", resonance " + juce::String(juce::roundToInt(pget(R))) + " (click = choose type, drag sideways = frequency, up or down = resonance; type order and Hz scale assumed)"); }
    juce::RangedAudioParameter *T, *F, *R; juce::Colour ring, curve; juce::String title; float last = -1, f0 = 0, r0 = 0;
};

// A text box of the mockup made into a drop-down list. The maker's document gives no value lists, so the controller value is taken as the position in the list (assumed).
class Dropdown : public juce::Component, public juce::SettableTooltipClient
{
public:
    Dropdown(juce::RangedAudioParameter* p, juce::StringArray opts, const TextRun& run, juce::Rectangle<float> box, const juce::String& name)
        : param(p), options(std::move(opts)), font(run.font), col(run.col), title(name)
    { setBounds(box.toNearestInt()); setMouseCursor(juce::MouseCursor::PointingHandCursor); tip(); }
    juce::String text() const { const int v = juce::roundToInt(pget(param)); return v < options.size() ? options[v] : juce::String(v); }
    void refresh() { if (text() != last) { repaint(); tip(); } }
    void paint(juce::Graphics& g) override { last = text(); g.setColour(col); g.setFont(font); g.drawText(last, getLocalBounds().toFloat(), juce::Justification::centred, false); }
    void mouseDown(const juce::MouseEvent&) override
    {
        if (!param) return; juce::PopupMenu m; const int cur = juce::roundToInt(pget(param));
        for (int i = 0; i < options.size(); ++i) m.addItem(i + 1, options[i], true, i == cur);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMinimumWidth(getWidth()), [this, safe = juce::Component::SafePointer<Dropdown>(this)](int r)
            { if (safe != nullptr && r > 0 && param) { param->beginChangeGesture(); pset(param, (float) (r - 1)); param->endChangeGesture(); repaint(); } });
    }
private:
    void tip() { setTooltip(title + ": " + text() + " (click to choose; the list order is assumed, not yet checked on a real UltraNova)"); }
    juce::RangedAudioParameter* param; juce::StringArray options; juce::Font font; juce::Colour col; juce::String title, last;
};

// LFO waveform names as in the maker's LFO Waveform Table; which controller value picks which name is assumed (value = position in this list).
// The six filter routing tiles (Bypass, Single, Series, Parallel 1, Parallel 2, Drum). Click one to choose; the list order is assumed (value = tile number), stated in the tooltip.
class RouteTiles : public juce::Component, public juce::SettableTooltipClient
{
public:
    RouteTiles(juce::RangedAudioParameter* p, std::vector<juce::Rectangle<float>> t) : param(p), tiles(std::move(t))
    {
        juce::Rectangle<float> u; for (auto& r : tiles) u = u.isEmpty() ? r : u.getUnion(r); origin = u.getPosition(); setBounds(u.toNearestInt()); setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setTooltip("Filter routing: click a tile to choose (the order of the list is assumed, not confirmed by the maker's document)");
    }
    int current() const { return juce::jlimit(0, (int) tiles.size() - 1, juce::roundToInt(pget(param))); }
    void refresh() { if (current() != last) repaint(); }
    void paint(juce::Graphics& g) override
    {
        last = current();
        for (size_t i = 0; i < tiles.size(); ++i)
        {
            const auto r = tiles[i].translated(-origin.x, -origin.y); const bool on = (int) i == last;
            g.setColour(juce::Colour(0xff0B0C10)); g.fillRoundedRectangle(r, 4); g.setColour(juce::Colour(on ? 0xff7EE3E8 : 0xff3A3D44)); g.drawRoundedRectangle(r.reduced(0.5f), 4, 1);
            const auto d = r.reduced(4).withHeight(26);
            g.setColour(juce::Colour(0xff555555)); g.drawRect(juce::Rectangle<float>(d.getX() + 0, d.getY() + 4, 50, 14), 1.0f);
            g.setColour(juce::Colour(0xff888888)); g.drawRect(juce::Rectangle<float>(d.getRight() - 20, d.getY() + 2, 14, 9), 1.0f);
            g.setColour(juce::Colour(i < 2 ? 0xff333333 : 0xff888888)); g.drawRect(juce::Rectangle<float>(d.getRight() - 20, d.getBottom() - 11, 14, 9), 1.0f);
            static const char* names[] = { "Bypass", "Single", "Series", "Parallel 1", "Parallel 2", "Drum" };
            g.setColour(juce::Colour(0xffA9AFBA)); g.setFont(makeFont(400, false, 9.0f)); g.drawText(names[juce::jmin((int) i, 5)], juce::Rectangle<float>(r.getX() + 4, r.getY() + 30, r.getWidth() - 8, 13), juce::Justification::centredLeft);
        }
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        for (size_t i = 0; i < tiles.size(); ++i) if (tiles[i].translated(-origin.x, -origin.y).contains(e.position)) { pset(param, (float) i); repaint(); return; }
    }
private:
    juce::RangedAudioParameter* param; std::vector<juce::Rectangle<float>> tiles; juce::Point<float> origin; int last = -1;
};
// Effects page: effect list of the selected slot, slot tiles (click to select, border follows) and the eight routing tiles (nrpn_0_97).
// Effect list order is assumed from the maker's picture of the list, not confirmed on a real UltraNova.
inline const juce::StringArray& fxNames()
{
    static const juce::StringArray n { "BYPASS", "EQ", "Compressor 1", "Compressor 2", "Distortion 1", "Distortion 2", "Delay 1", "Delay 2", "Reverb 1", "Reverb 2", "Chorus 1", "Chorus 2", "Chorus 3", "Chorus 4", "Gator" };
    return n;
}
class FxEditor : public juce::Component, public juce::SettableTooltipClient
{
public:
    // slot[0..4]: effect select params; route: configuration param; the rectangles are in the same design space as the page
    FxEditor(std::array<juce::RangedAudioParameter*, 5> s, juce::RangedAudioParameter* r, juce::Rectangle<float> listBox, std::array<juce::Rectangle<float>, 5> tiles, std::array<juce::Rectangle<float>, 8> routes, int* selSlot)
        : slots(s), route(r), list(listBox), tl(tiles), rt(routes), sel(selSlot)
    {
        juce::Rectangle<float> u = list; for (auto& t : tl) u = u.getUnion(t); for (auto& t : rt) u = u.getUnion(t);
        origin = u.getPosition(); setBounds(u.toNearestInt()); setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setTooltip("Effects: click a slot to edit it, click an effect to put it in that slot, click a routing to choose it (list orders are assumed, not checked on a real UltraNova)");
    }
    int cur() const { return juce::jlimit(0, 4, *sel); }
    int effect() const { return juce::jlimit(0, fxNames().size() - 1, juce::roundToInt(pget(slots[(size_t) cur()]))); }
    juce::Rectangle<float> cell(int i) const   // BYPASS spans both columns, the rest is two columns
    {
        const float w = list.getWidth() - 6, rowH = 14.0f, x0 = list.getX() + 3 - origin.x, y0 = list.getY() + 3 - origin.y;
        if (i == 0) return { x0, y0, w, rowH };
        const int k = i - 1; return { x0 + (k % 2) * w / 2, y0 + rowH + (float) (k / 2) * rowH, w / 2, rowH };
    }
    void refresh() { const int h = effect() * 1000 + cur() * 100 + juce::roundToInt(pget(route)); int sig = h; for (auto* q : slots) sig = sig * 31 + juce::roundToInt(pget(q)); if (sig != last) repaint(); }
    void paint(juce::Graphics& g) override
    {
        int sig = effect() * 1000 + cur() * 100 + juce::roundToInt(pget(route)); for (auto* q : slots) sig = sig * 31 + juce::roundToInt(pget(q)); last = sig;
        g.setColour(juce::Colour(0xff070809)); g.fillRect(list.translated(-origin.x, -origin.y).reduced(1));
        g.setFont(makeFont(400, false, 10.0f));
        for (int i = 0; i < fxNames().size(); ++i)
        {
            const auto c = cell(i).reduced(0.5f); const bool on = i == effect();
            if (on) { g.setColour(juce::Colour(0xff2C3448)); g.fillRect(c); }
            g.setColour(juce::Colour(on ? 0xffFFFFFF : 0xff8F949E)); g.drawText(fxNames()[i], c.reduced(4, 0), i == 0 ? juce::Justification::centred : juce::Justification::centredLeft, false);
        }
        for (int i = 0; i < 5; ++i)   // slot border over the baked one
        {
            const auto r = tl[(size_t) i].translated(-origin.x, -origin.y).expanded(1.5f);
            g.setColour(juce::Colour(0xff2F4570)); g.drawRoundedRectangle(r.expanded(1.0f), 7, 2.5f);
            g.setColour(juce::Colour(i == cur() ? 0xff7EE3E8 : 0xff45474D)); g.drawRoundedRectangle(r, 6, 1.2f);
        }
        const int rc = juce::jlimit(0, 7, juce::roundToInt(pget(route)));
        for (int i = 0; i < 8; ++i)
        {
            const auto r = rt[(size_t) i].translated(-origin.x, -origin.y);
            g.setColour(juce::Colour(0xff1C2036)); g.drawRoundedRectangle(r.expanded(1.0f), 4, 2.5f);   // covers the border baked into the art
            g.setColour(juce::Colour(i == rc ? 0xff7EE3E8 : 0xff3A3D44)); g.drawRoundedRectangle(r.reduced(0.5f), 3, i == rc ? 1.5f : 1.0f);
        }
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        const auto pt = e.position;
        for (int i = 0; i < 8; ++i) if (rt[(size_t) i].translated(-origin.x, -origin.y).contains(pt)) { pset(route, (float) i); repaint(); return; }
        for (int i = 0; i < fxNames().size(); ++i) if (cell(i).contains(pt)) { auto* q = slots[(size_t) cur()]; q->beginChangeGesture(); pset(q, (float) i); q->endChangeGesture(); repaint(); return; }
        for (int i = 0; i < 5; ++i) if (tl[(size_t) i].translated(-origin.x, -origin.y).getUnion(tl[(size_t) i].translated(-origin.x, -origin.y)).contains(pt)) { *sel = i; repaint(); return; }
    }
private:
    std::array<juce::RangedAudioParameter*, 5> slots; juce::RangedAudioParameter* route; juce::Rectangle<float> list; std::array<juce::Rectangle<float>, 5> tl; std::array<juce::Rectangle<float>, 8> rt; int* sel; juce::Point<float> origin; int last = -1;
};
// One bit of a packed controller value (for example Arpeggiator On / Latch, Vocoder On / sibilance type). Click toggles it. The bit positions come from the maker's document.
class BitSwitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    BitSwitch(juce::RangedAudioParameter* p, int bitNo, juce::Rectangle<float> box, const juce::String& name, juce::StringArray texts = {}, juce::Font f = {}, juce::Colour c = juce::Colours::white, bool lamp = false)
        : param(p), bit(bitNo), txt(std::move(texts)), font(f), col(c), isLamp(lamp), title(name)
    { setBounds(box.toNearestInt()); setMouseCursor(juce::MouseCursor::PointingHandCursor); setTooltip(name + ": click to switch (one bit of a packed value, the maker's document gives the bit but not the labels)"); }
    bool on() const { return (juce::roundToInt(pget(param)) >> bit) & 1; }
    void refresh() { if ((int) on() != last) repaint(); }
    void paint(juce::Graphics& g) override
    {
        last = (int) on();
        if (isLamp) { if (last) { g.setColour(juce::Colour(0xff7EE3E8)); g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(1), 5); g.setColour(juce::Colour(0xff0B0C10)); g.setFont(makeFont(500, false, 8.0f)); g.drawText("ON", getLocalBounds(), juce::Justification::centred); } return; }
        if (txt.size() >= 2) { g.setColour(col); g.setFont(font); g.drawText(txt[last], getLocalBounds().toFloat(), juce::Justification::centred, false); }
    }
    void mouseDown(const juce::MouseEvent&) override
    { if (!param) return; int v = juce::roundToInt(pget(param)); v ^= (1 << bit); param->beginChangeGesture(); pset(param, (float) v); param->endChangeGesture(); repaint(); }
private:
    juce::RangedAudioParameter* param; int bit; juce::StringArray txt; juce::Font font; juce::Colour col; bool isLamp; juce::String title; int last = -1;
};
inline const juce::StringArray& lfoNames()
{
    static const juce::StringArray n { "Sine", "Triangle", "Sawtooth", "Square", "Rand S/H", "Time S/H", "PianoEnv", "Seq 1", "Seq 2", "Seq 3", "Seq 4", "Seq 5", "Seq 6", "Seq 7",
        "Altern 1", "Altern 2", "Altern 3", "Altern 4", "Altern 5", "Altern 6", "Altern 7", "Altern 8", "Chromat", "Major", "Major 7", "Minor 7", "MinArp 1", "MinArp 2", "Diminish", "DecMinor", "Minor3rd", "Pedal", "4ths", "4ths x12", "1625 Maj", "1625 Min", "2511" };
    return n;
}
// The LFO waveform window. Smooth shapes are drawn as lines, stepped shapes as bars; the stepped patterns are illustrative (the real step values are not in the maker's document).
class LfoWindow : public juce::Component, public juce::SettableTooltipClient
{
public:
    LfoWindow(juce::RangedAudioParameter* p, juce::Rectangle<float> box) : param(p) { setBounds(box.toNearestInt()); setMouseCursor(juce::MouseCursor::PointingHandCursor); tip(); }
    void refresh() { if (pget(param) != last) { repaint(); tip(); } }
    void paint(juce::Graphics& g) override
    {
        last = pget(param); const int idx = juce::jlimit(0, lfoNames().size() - 1, juce::roundToInt(last)); const auto b = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(juce::Colour(0xff0A1626)); g.fillRoundedRectangle(b, 4); g.setColour(juce::Colour(0xff4E5F86)); g.drawRoundedRectangle(b, 4, 1);
        const float W = (float) getWidth(), top = 8, bot = (float) getHeight() - 16, mid = (top + bot) / 2, amp = (bot - top) / 2;
        if (idx <= 3 || idx == 6)
        {
            juce::Path p; for (int i = 0; i <= 96; ++i)
            {
                const float t = (float) i / 96.0f * 2.0f, f = t - std::floor(t); float v;
                if (idx == 0) v = std::sin(juce::MathConstants<float>::twoPi * f); else if (idx == 1) v = f < 0.5f ? 4 * f - 1 : 3 - 4 * f; else if (idx == 2) v = 2 * f - 1; else if (idx == 3) v = f < 0.5f ? 1.0f : -1.0f; else v = 2 * std::exp(-4.0f * f) - 1;
                const float x = 10 + t / 2.0f * (W - 20), y = mid - v * amp; if (i) p.lineTo(x, y); else p.startNewSubPath(x, y);
            }
            g.setColour(juce::Colour(0xff7EE3E8)); g.strokePath(p, juce::PathStrokeType(2.0f));
        }
        else
        {
            float x = (float) (idx * 37 + 11); auto rnd = [&x] { x = std::fmod(x * 9301.0f + 49297.0f, 233280.0f); return x / 233280.0f; };
            const int n = idx >= 14 && idx < 22 ? 8 : 8; const float bw = (W - 20) / (float) n; g.setColour(juce::Colour(0xffE6E8EC));
            for (int i = 0; i < n; ++i)
            {
                float v = rnd(); if (idx >= 14 && idx < 22) v = (i % 2 == 0) ? 0.85f : 0.2f + 0.15f * (float) ((idx + i) % 3); else if (idx >= 22) v = 0.2f + 0.8f * (float) ((i * (idx % 5 + 2) + idx) % 7) / 6.0f;
                const float h = v * (bot - top); g.fillRect(10 + (float) i * bw + 1, bot - h, bw - 2, h);
            }
        }
        g.setColour(juce::Colour(0xff99AAAA)); g.setFont(makeFont(400, false, 7.0f)); g.drawText(lfoNames()[idx], getLocalBounds().withTrimmedTop(getHeight() - 14), juce::Justification::centred);
    }
    void mouseDown(const juce::MouseEvent&) override
    {
        if (!param) return; juce::PopupMenu m; const int cur = juce::roundToInt(pget(param));
        for (int i = 0; i < lfoNames().size(); ++i) m.addItem(i + 1, lfoNames()[i], true, i == cur);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this), [this, safe = juce::Component::SafePointer<LfoWindow>(this)](int r)
            { if (safe != nullptr && r > 0 && param) { param->beginChangeGesture(); pset(param, (float) (r - 1)); param->endChangeGesture(); repaint(); } });
    }
private:
    void tip() { setTooltip(juce::String("LFO waveform: ") + lfoNames()[juce::jlimit(0, lfoNames().size() - 1, juce::roundToInt(pget(param)))] + " (click to choose; list order assumed, stepped patterns illustrative)"); }
    juce::RangedAudioParameter* param; float last = -1;
};

// A knob wired to one parameter (or disabled at the mockup's resting angle when there is none).
// The modulation matrix rows. Names come from the maker's Sources table (display names) and Destination table; which controller value picks which name is assumed (value = position in the list)
// and only the entries printed in the tables available to us are named, the rest show as numbers.
inline const juce::StringArray& modSourceNames()
{
    static const juce::StringArray n { "Direct", "ModWheel", "AftTouch", "Express", "Velocity", "Keyboard", "Lfo1+", "Lfo2+", "Lfo3+", "Lfo1+/-", "Lfo2+/-", "Lfo3+/-", "Env1Amp", "Env2Filt", "Env3", "Env4", "Env5", "Env6" };
    return n;
}
inline const juce::StringArray& modDestNames()
{
    static const juce::StringArray n { "O123Ptch", "O1Pitch", "O2Pitch", "O3Pitch", "O1Vsync", "O2Vsync", "O3Vsync", "O1PW/Idx", "O2PW/Idx", "O3PW/Idx", "O1Hard", "O2Hard", "O3Hard",
        "O1Level", "O2Level", "O3Level", "NoiseLvl", "RM1*3Lvl", "RM2*3Lvl", "F1DAmnt", "F2DAmnt", "F1Freq", "F2Freq", "F1Res", "F2Res", "FBalance", "L1Rate", "L2Rate", "L3Rate", "Env1Dec", "Env2Dec",
        "FX1Amnt", "FX2Amnt", "FX3Amnt", "FX4Amnt", "FX5Amnt", "FXFedback", "FXWetDry", "Ch1Rate", "Ch1Depth", "Ch1Delay", "Ch1Fback", "Ch2Rate", "Ch2Depth", "Ch2Delay", "Ch2Fback", "Ch3Rate", "Ch3Depth", "Ch3Delay", "Ch3Fback",
        "Ch4Rate", "Ch4Depth", "Ch4Delay", "Ch4Fback", "Dly1Time", "Dly1Fbak", "Dly2Time", "Dly2Fbak", "EQBasLvl", "EQBasFrq", "EQMidLvl", "EQMidFrq", "EQTrbLvl", "EQTrbFrq", "PanPosn" };
    return n;
}
class MatrixRows : public juce::Component, public juce::TooltipClient
{
public:
    MatrixRows(std::vector<MatrixRowSpec> r, const std::function<juce::RangedAudioParameter*(const juce::String&)>& find) : rows(std::move(r))
    {
        juce::Rectangle<float> u; for (auto& m : rows) for (auto& c : m.c) u = u.isEmpty() ? c : u.getUnion(c); origin = u.getPosition(); setBounds(u.getSmallestIntegerContainer());
        for (auto& m : rows)
        {
            const int k = m.slot - 1; const int msb = k < 9 ? 1 : 2, base = k < 9 ? 83 + 5 * k : 5 * (k - 9);
            std::array<juce::RangedAudioParameter*, 5> a; for (int j = 0; j < 5; ++j) a[(size_t) j] = find("nrpn_" + juce::String(msb) + "_" + juce::String(base + j));
            ps.push_back({ a[0], a[1], a[2], a[4], a[3] });   // source 1, source 2, touch, destination, depth (parameter order is source1, source2, touch, depth, destination)
        }
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }
    void refresh() { const auto sig = signature(); if (sig != last) { last = sig; repaint(); } }
    void paint(juce::Graphics& g) override
    {
        last = signature();
        for (size_t i = 0; i < rows.size(); ++i)
        {
            auto cell = [&](int j) { return rows[i].c[j].translated(-origin.x, -origin.y); };
            const int s1 = val(i, 0), s2 = val(i, 1), t = val(i, 2), d = val(i, 3), dp = val(i, 4);
            g.setFont(makeFont(400, false, 12.0f));
            g.setColour(juce::Colour(s1 == 0 ? 0xff667788 : 0xffEEEEFF)); g.drawText(srcName(s1), cell(0), juce::Justification::centredLeft, false);
            g.setColour(juce::Colour(s2 == 0 ? 0xff667788 : 0xffEEEEFF)); g.drawText(juce::String(juce::CharPointer_UTF8("\xc3\x97 ")) + srcName(s2), cell(1), juce::Justification::centredLeft, false);
            g.setFont(makeFont(400, false, 11.0f)); g.setColour(juce::Colour(t == 0 ? 0xff667788 : 0xffEEEEFF)); g.drawText(t == 0 ? juce::String("None") : juce::String(t), cell(2), juce::Justification::centredLeft, false);
            auto db = cell(3); g.setGradientFill(juce::ColourGradient(juce::Colour(0xff0D1F78), 0, db.getY(), juce::Colour(0xff1E3FB8), 0, db.getBottom(), false)); g.fillRoundedRectangle(db, 3);
            g.setColour(juce::Colour(0xff08103A)); g.drawRoundedRectangle(db.reduced(0.5f), 3, 1);
            g.setColour(juce::Colour(0xff8DB8FF)); g.setFont(makeFont(400, true, 17.0f)); g.drawText(destName(d), db.reduced(8, 0), juce::Justification::centredLeft, true);
            auto bar = cell(4).withSizeKeepingCentre(cell(4).getWidth(), 10.0f); g.setGradientFill(juce::ColourGradient(juce::Colour(0xff1B2B6E), bar.getX(), 0, juce::Colour(0xff4D7BFF), bar.getRight(), 0, false)); g.fillRect(bar);
            g.setColour(juce::Colours::white); g.fillRect(bar.getX() + (float) dp / 127.0f * bar.getWidth() - 1.0f, bar.getY() - 2, 3.0f, 14.0f);
        }
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        const auto hit = locate(e.position); row = hit.first; col = hit.second; if (row < 0) return;
        auto* p = ps[(size_t) row][(size_t) col]; if (!p) return; dragStart = pget(p); dragY = e.position.y;
        if (col == 0 || col == 1 || col == 3) menu(row, col); else if (col == 4) { p->beginChangeGesture(); setDepth(e.position.x); }
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (row < 0) return; auto* p = ps[(size_t) row][(size_t) col]; if (!p) return;
        if (col == 4) setDepth(e.position.x); else if (col == 2) { p->beginChangeGesture(); pset(p, dragStart + (dragY - e.position.y) * 0.5f); p->endChangeGesture(); repaint(); }
    }
    void mouseUp(const juce::MouseEvent&) override { if (row >= 0 && col == 4) if (auto* p = ps[(size_t) row][4]) p->endChangeGesture(); }
    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        const auto hit = locate(e.position); if (hit.first < 0) return; auto* p = ps[(size_t) hit.first][(size_t) hit.second]; if (!p) return;
        if (hit.second == 4) { p->beginChangeGesture(); pset(p, 64); p->endChangeGesture(); repaint(); } else if (hit.second == 2) { p->beginChangeGesture(); pset(p, 0); p->endChangeGesture(); repaint(); }
    }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const auto hit = locate(e.position); if (hit.first < 0 || hit.second == 0 || hit.second == 1 || hit.second == 3) return; auto* p = ps[(size_t) hit.first][(size_t) hit.second]; if (!p) return;
        p->beginChangeGesture(); pset(p, pget(p) + (w.deltaY > 0 ? 2.0f : -2.0f)); p->endChangeGesture(); repaint();
    }
    juce::String getTooltip() override
    {
        const auto hit = locate(getMouseXYRelative().toFloat()); if (hit.first < 0) return {};
        const juce::String what[] = { "Source 1", "Source 2", "Touch enable (raw value, its meaning is not in the maker's document)", "Destination", "Depth (-64 to +63)" };
        const int slot = rows[(size_t) hit.first].slot; const bool list = hit.second == 0 || hit.second == 1 || hit.second == 3;
        return "Slot " + juce::String(slot) + ", " + what[hit.second] + (list ? ": click to choose (the list order is assumed and only the entries printed in the maker's tables are named)" : hit.second == 4 ? ": drag, wheel, double click resets to 0" : ": drag up or down, wheel, double click resets");
    }
private:
    std::pair<int, int> locate(juce::Point<float> pt) const
    {
        for (size_t i = 0; i < rows.size(); ++i) for (int j = 0; j < 5; ++j) if (rows[i].c[j].translated(-origin.x, -origin.y).expanded(2, 1).contains(pt)) return { (int) i, j };
        return { -1, -1 };
    }
    int val(size_t i, int j) const { return juce::roundToInt(pget(ps[i][(size_t) j])); }
    juce::String srcName(int v) const { return v < modSourceNames().size() ? modSourceNames()[v] : "Src " + juce::String(v); }
    juce::String destName(int v) const { return v < modDestNames().size() ? modDestNames()[v] : "Dest " + juce::String(v); }
    juce::String signature() const { juce::String s; for (size_t i = 0; i < rows.size(); ++i) for (int j = 0; j < 5; ++j) s << val(i, j) << ','; return s; }
    void setDepth(float x)
    {
        auto* p = ps[(size_t) row][4]; const auto c = rows[(size_t) row].c[4].translated(-origin.x, -origin.y);
        pset(p, juce::jlimit(0.0f, 1.0f, (x - c.getX()) / c.getWidth()) * 127.0f); repaint();
    }
    void menu(int r, int c)
    {
        auto* p = ps[(size_t) r][(size_t) c]; const bool isDest = c == 3; const int count = isDest ? modDestNames().size() : modSourceNames().size(); const int cur = juce::roundToInt(pget(p));
        juce::PopupMenu m; for (int i = 0; i < count; ++i) m.addItem(i + 1, isDest ? modDestNames()[i] : modSourceNames()[i], true, i == cur);
        if (cur >= count) m.addItem(cur + 1, juce::String(isDest ? "Dest " : "Src ") + juce::String(cur), true, true);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea(localAreaToGlobal(rows[(size_t) r].c[(size_t) c].translated(-origin.x, -origin.y).toNearestInt())).withMaximumNumColumns(isDest ? 4 : 1),
            [this, p, safe = juce::Component::SafePointer<MatrixRows>(this)](int res) { if (safe != nullptr && res > 0) { p->beginChangeGesture(); pset(p, (float) (res - 1)); p->endChangeGesture(); repaint(); } });
    }
    std::vector<MatrixRowSpec> rows; std::vector<std::array<juce::RangedAudioParameter*, 5>> ps; juce::Point<float> origin; juce::String last; int row = -1, col = -1; float dragStart = 0, dragY = 0;
};
class Knob : public juce::Slider
{
public:
    Knob(juce::RangedAudioParameter* p, float mockAngle, juce::LookAndFeel& laf) : param(p)
    {
        setLookAndFeel(&laf); setSliderStyle(juce::Slider::RotaryVerticalDrag); setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        setRotaryParameters(juce::degreesToRadians(225.0f), juce::degreesToRadians(495.0f), true); setMouseDragSensitivity(220); setScrollWheelEnabled(true);
        if (p) { attach = std::make_unique<juce::SliderParameterAttachment>(*p, *this); setDoubleClickReturnValue(true, p->convertFrom0to1(p->getDefaultValue())); }
        else { setRange(0.0, 1.0, 0.0); setValue((mockAngle + 135.0f) / 270.0f, juce::dontSendNotification); setEnabled(false); }
        updateTip();
    }
    ~Knob() override { setLookAndFeel(nullptr); }
    void valueChanged() override { updateTip(); }
    void updateTip()
    {
        if (!param) { setTooltip("Not connected yet: the maker's document does not list a controller for this knob."); return; }
        setTooltip(param->getName(60) + ": " + param->getText(param->getValue(), 20) + " (double click resets)");
    }
private:
    juce::RangedAudioParameter* param; std::unique_ptr<juce::SliderParameterAttachment> attach;
};

// The page: artwork at the back, knobs on top, menu on the left, buttons in the header.
class Content : public juce::Component, private juce::Timer
{
public:
    std::function<void()> onZoom, onInfo, onAll, onReset100;
    Content(juce::AudioProcessor& p) : proc(p) { setSize(kW, kH); setOpaque(true); startTimerHz(20); }
    ~Content() override { stopTimer(); knobs.clear(); fields.clear(); faders.clear(); bars.clear(); envGraph.reset(); filterDials.clear(); dropdowns.clear(); lfoViews.clear(); routeView.reset(); matrixView.reset(); fxView.reset(); bitSwitches.clear(); }

    void showPage(const juce::String& key)
    {
        knobs.clear(); dials.clear(); fields.clear(); faders.clear(); bars.clear(); envGraph.reset(); filterDials.clear(); dropdowns.clear(); lfoViews.clear(); routeView.reset(); matrixView.reset(); fxView.reset(); bitSwitches.clear(); live.clear(); skip.clear(); shown.clear(); picker.reset(); page = key; data = PageData::load(key); const auto& ids = knobMap().at(key);
        for (size_t i = 0; i < data->knobs.size(); ++i)
        {
            juce::RangedAudioParameter* prm = nullptr;
            if (i < ids.size() && ids[i].isNotEmpty())
                for (auto* q : proc.getParameters()) if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(q)) if (r->paramID == ids[i]) { prm = r; break; }
            auto k = std::make_unique<Knob>(prm, data->knobs[i].angle, look); k->setBounds(data->knobs[i].rect.toNearestInt()); addAndMakeVisible(*k); knobs.push_back(std::move(k));
        }
        static const char* waveIds[3] = { "cc_19", "cc_29", "cc_41" };
        for (size_t i = 0; i < data->dials.size() && i < 3; ++i)
        {
            juce::RangedAudioParameter* prm = nullptr; for (auto* q : proc.getParameters()) if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(q)) if (r->paramID == waveIds[i]) { prm = r; break; }
            auto d = std::make_unique<WaveDial>(prm, data->dials[i].color); static const char* pwIds[3] = { "cc_21", "cc_31", "cc_43" }; d->followShape(find(pwIds[i])); d->setBounds(data->dials[i].rect.toNearestInt()); d->onOpen = [this](WaveDial& wd) { openPicker(wd); }; addAndMakeVisible(*d); dials.push_back(std::move(d));
        }
        bindLive(key); repaint();
    }
    juce::RangedAudioParameter* find(const juce::String& id) const
    { for (auto* q : proc.getParameters()) if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(q)) if (r->paramID == id) return r; return nullptr; }
    // Text of the page that follows a control: value boxes (Field) and summary lines. Oscillators page first.
    void bindLive(const juce::String& key)
    {
        if (!data) return;
        // the menu rows "Oscillator 1..3" show semitone and cents of that oscillator, on every page
        static const char* semi[3] = { "cc_26", "cc_37", "cc_48" }; static const char* cents[3] = { "cc_27", "cc_39", "cc_49" }; int ns = 0, nc = 0, nf = 0;
        for (size_t i = 0; i < data->runs.size(); ++i)
        {
            const auto& r = data->runs[i]; if (r.r.getX() > 330) continue;
            if (r.t.endsWith(" Semi") && ns < 3) { if (auto* p = find(semi[ns++])) live[(int) i] = [p] { return Field::text(p, true) + " Semi"; }; }
            else if (r.t.length() >= 4 && (r.t.startsWith("LP") || r.t.startsWith("BP") || r.t.startsWith("HP")) && r.t.containsAnyOf("0123456789") && nf < 2) { if (auto* p = find(nf++ == 0 ? "cc_68" : "cc_82")) live[(int) i] = [p] { return juce::String(filterType(juce::roundToInt(pget(p))).name); }; }
            else if (r.t.endsWith(" Detune") && nc < 3) { if (auto* p = find(cents[nc++])) live[(int) i] = [p] { return Field::text(p, true) + " Detune"; }; }
        }
        static const juce::StringArray drive { "Diode", "Valve", "Clipper", "XOver", "Rectify", "BitsDown", "RateDown" };   // names as in the maker's Distortion list
        if (key == "mod") { bindMixerAndEnvelope(2); for (auto& mrow : data->matrixRows) for (auto& cc : mrow.c) for (size_t ri = 0; ri < data->runs.size(); ++ri) if (cc.expanded(3, 3).contains(data->runs[ri].r.getCentre())) skip.insert((int) ri);
        if (!data->matrixRows.empty()) { matrixView = std::make_unique<MatrixRows>(data->matrixRows, [this](const juce::String& id) { return find(id); }); addAndMakeVisible(*matrixView); } if (!data->lfoWins.empty()) { auto l = std::make_unique<LfoWindow>(find("nrpn_0_70"), data->lfoWins[0]); addAndMakeVisible(*l); lfoViews.push_back(std::move(l)); } return; }
        if (key == "fx")
        {
            // EQ values under the six knobs (levels centred, frequencies raw) and the title follow the selected slot
            const int eqRun[6] = { 84, 86, 88, 90, 92, 94 }; const char* eqId[6] = { "nrpn_0_105", "nrpn_0_107", "nrpn_0_109", "nrpn_0_104", "nrpn_0_106", "nrpn_0_108" };
            for (int k = 0; k < 6; ++k) if (auto* p = find(eqId[k])) { const bool centred = k < 3; if ((size_t) eqRun[k] < data->runs.size()) live[eqRun[k]] = [p, centred] { return Field::text(p, centred); }; }
            std::array<juce::RangedAudioParameter*, 5> sl {}; bool ok = true; for (int i = 0; i < 5; ++i) { sl[(size_t) i] = find("nrpn_0_" + juce::String(99 + i)); ok = ok && sl[(size_t) i]; }
            auto* rp = find("nrpn_0_97");
            if (ok && rp)
            {
                for (size_t ri = 0; ri < data->runs.size(); ++ri)
                {
                    const auto& r = data->runs[ri]; const auto c = r.r.getCentre();
                    if (juce::Rectangle<float>(315, 45, 197, 118).contains(c) && ri != 67) skip.insert((int) ri);                              // old effect list
                    if (juce::Rectangle<float>(510, 250, 700, 24).contains(c)) skip.insert((int) ri);                                           // slot effect names, drawn by the pickers below
                }
                live[67] = [this] { return juce::String::fromUTF8("Effect Edit \xc2\xb7 Slot ") + juce::String(fxSel + 1); };
                std::array<juce::Rectangle<float>, 5> tiles; for (int i = 0; i < 5; ++i) tiles[(size_t) i] = { 515.0f + 122.7f * (float) i, 195.0f, 108.0f, 82.0f };
                std::array<juce::Rectangle<float>, 8> routes; for (int i = 0; i < 8; ++i) routes[(size_t) i] = { 303.0f + 252.0f * (float) (i % 4), 303.0f + 34.0f * (float) (i / 4), 246.0f, 27.0f };
                fxView = std::make_unique<FxEditor>(sl, rp, juce::Rectangle<float>(315, 45, 197, 118), tiles, routes, &fxSel); addAndMakeVisible(*fxView);
                for (int i = 0; i < 5; ++i) if (auto* q = sl[(size_t) i])
                {
                    const juce::Rectangle<float> b(tiles[(size_t) i].getX() + 5, 253, 98, 20); TextRun run; run.font = makeFont(400, false, 10.0f); run.col = juce::Colour(0xffDCE3F0);
                    auto d = std::make_unique<Dropdown>(q, fxNames(), run, b, "Slot " + juce::String(i + 1) + " effect"); addAndMakeVisible(*d); dropdowns.push_back(std::move(d));
                }
            }
            // Arpeggiator, vocoder and pan controls below the routing
            {
                auto bits = [this](const char* id, int bit, juce::Rectangle<float> box, const juce::String& name, juce::StringArray t = {}, int run = -1, bool lamp = false) {
                    auto* q = find(id); if (!q) return; juce::Font f = makeFont(400, false, 9.0f); juce::Colour c(0xffDCE3F0); if (run >= 0) { f = data->runs[(size_t) run].font; c = data->runs[(size_t) run].col; skip.insert(run); }
                    auto b = std::make_unique<BitSwitch>(q, bit, box, name, t, f, c, lamp); addAndMakeVisible(*b); bitSwitches.push_back(std::move(b)); };
                const auto tw = [this](int run, float extra) { auto r = data->runs[(size_t) run].r; return r.expanded(extra, 1.0f); };
                auto onText = [this](int run, const char* word, const char* id) { auto* q = find(id); if (q && (size_t) run < data->runs.size()) live[run] = [q, word] { return juce::String(word) + (((juce::roundToInt(pget(q))) & 1) ? juce::String::fromUTF8(" \xe2\x97\x89 On") : juce::String::fromUTF8(" \xe2\x97\xaf On")); }; };
                onText(143, "Arpeggiator", "nrpn_1_61"); onText(158, "Vocoder", "nrpn_1_55");
                bits("nrpn_1_61", 0, tw(143, 4), "Arpeggiator on"); bits("nrpn_1_55", 0, tw(158, 4), "Vocoder on");
                bits("nrpn_1_61", 2, { 411, 436, 26, 13 }, "Arpeggiator latch", {}, -1, true);
                bits("nrpn_1_55", 1, tw(163, 12), "Vocoder sibilance type", { "High Pass", "Noise" }, 163);
                auto box = [this](int run, const char* id, bool centred, float w) { auto* q = find(id); if (!q) return; const auto& r = data->runs[(size_t) run]; skip.insert(run);
                    auto f = std::make_unique<Field>(q, centred, r, juce::Rectangle<float>(w, 16).withCentre(r.r.getCentre())); addAndMakeVisible(*f); fields.push_back(std::move(f)); };
                box(146, "nrpn_1_62", false, 34); box(172, "cc_89", false, 30);
                dropdown("Up", "nrpn_1_65", { "Up", "Down", "Alt 1", "Alt 2", "Random", "Played", "Chord" }, "Arpeggiator mode");
            }
            return;
        }
        if (key == "filter") { bindMixerAndEnvelope(1); if (!data->routeTiles.empty()) { if (auto* rp = find("cc_60")) { routeView = std::make_unique<RouteTiles>(rp, data->routeTiles); addAndMakeVisible(*routeView); } } dropdown("Diode", "cc_65", drive, "Filter 1 drive type"); dropdown("XOver", "cc_81", drive, "Filter 2 drive type"); return; }
        if (key != "osc") return;
        auto box = [this](int run, const char* id, bool centred) {
            auto* p = find(id); if (!p) return; const auto& r = data->runs[(size_t) run]; skip.insert(run);
            auto f = std::make_unique<Field>(p, centred, r, juce::Rectangle<float>(40, 20).withCentre(r.r.getCentre())); addAndMakeVisible(*f); fields.push_back(std::move(f)); };
        box(78, "cc_26", true); box(79, "cc_28", true); box(104, "cc_37", true); box(105, "cc_40", true); box(130, "cc_48", true); box(131, "cc_50", true);
        box(171, "cc_14", false);
        bindMixerAndEnvelope(0);
        auto dropAt = [this](int run, const char* id, juce::StringArray opts, const juce::String& name) {   // drop-down on the run with this index (the text of the box is not unique on this page)
            auto* q = find(id); if (!q || (size_t) run >= data->runs.size()) return; const auto& r = data->runs[(size_t) run]; skip.insert(run);
            auto d = std::make_unique<Dropdown>(q, std::move(opts), r, juce::Rectangle<float>(juce::jmax(56.0f, r.r.getWidth() + 18.0f), 18).withCentre(r.r.getCentre()), name); addAndMakeVisible(*d); dropdowns.push_back(std::move(d)); };
        dropAt(148, "cc_17", { "Free", "0", "45", "90", "135", "180", "225", "270", "315" }, "Oscillator start phase");
        dropAt(150, "cc_18", { "Off", "On" }, "Fixed note");
        dropAt(158, "cc_57", { "White", "Pink", "Red", "Blue" }, "Noise type");
        dropdown("Exponential", "cc_12", { "Linear", "Exponential" }, "Portamento mode"); dropdown("Mono Auto-Glide", "cc_3", { "Mono", "Mono Auto-Glide", "Polyphonic" }, "Polyphony mode");
    }
    void dropdown(const juce::String& boxText, const char* id, juce::StringArray opts, const juce::String& name)
    {
        auto* p = find(id); if (!p) return;
        for (size_t i = 0; i < data->runs.size(); ++i) if (data->runs[i].t == boxText && !skip.count((int) i) && data->runs[i].r.getX() > 330)
        {
            const auto& r = data->runs[i]; skip.insert((int) i); auto d = std::make_unique<Dropdown>(p, opts, r, juce::Rectangle<float>(juce::jmax(56.0f, r.r.getWidth() + 18.0f), 18).withCentre(r.r.getCentre()), name);
            addAndMakeVisible(*d); dropdowns.push_back(std::move(d)); return;
        }
    }
    // Mixer faders, the six envelope bars and the envelope graph (same layout on the Oscillators page for the amp envelope and on the Filter page for the filter envelope).
    void bindMixerAndEnvelope(int page)
    {
        const bool amp = page == 0, mod = page == 2;
        static const char* mix[6] = { "cc_51", "cc_52", "cc_53", "cc_54", "cc_55", "cc_56" }; static const char* mixName[6] = { "Osc 1 level", "Osc 2 level", "Osc 3 level", "Ring mod 1*3 level", "Ring mod 2*3 level", "Noise level" };
        for (size_t i = 0; i < data->faders.size() && i < 6; ++i) { auto f = std::make_unique<Fader>(find(mix[i]), mixName[i], data->faders[i]); addAndMakeVisible(*f); faders.push_back(std::move(f)); }
        static const char* ampBar[6] = { "cc_115", "cc_116", "cc_112", "cc_113", "cc_109", "cc_110" }; static const char* filBar[6] = { "nrpn_0_11", "nrpn_0_12", "nrpn_0_8", "nrpn_0_9", "nrpn_0_5", "nrpn_0_6" };
        static const char* modBar[6] = { "nrpn_0_67", "nrpn_0_68", "nrpn_0_64", "nrpn_0_65", "nrpn_0_61", "nrpn_0_62" };
        static const char* bname[6] = { "attack slope", "decay slope", "attack track", "decay track", "sustain rate", "sustain time" };
        for (size_t i = 0; i < data->bars.size() && i < 6; ++i) { auto b = std::make_unique<Bar>(find(amp ? ampBar[i] : mod ? modBar[i] : filBar[i]), juce::String(amp ? "Amp env " : mod ? "Envelope 6 " : "Filter env ") + bname[i], data->bars[i]); addAndMakeVisible(*b); bars.push_back(std::move(b)); }
        if (!data->env.isEmpty())
        {
            envGraph = amp ? std::make_unique<EnvGraph>(find("cc_73"), find("cc_75"), find("cc_70"), find("cc_110"), find("cc_72"), data->env) : mod ? std::make_unique<EnvGraph>(find("nrpn_0_57"), find("nrpn_0_58"), find("nrpn_0_59"), find("nrpn_0_62"), find("nrpn_0_60"), data->env) : std::make_unique<EnvGraph>(find("nrpn_0_1"), find("nrpn_0_2"), find("nrpn_0_3"), find("nrpn_0_6"), find("nrpn_0_4"), data->env);
            addAndMakeVisible(*envGraph);
        }
        if (page == 1)
        {
            static const char* t[2] = { "cc_68", "cc_82" }; static const char* f[2] = { "cc_74", "cc_83" }; static const char* r[2] = { "cc_71", "cc_85" };
            for (size_t i = 0; i < data->filterDials.size() && i < 2; ++i) { auto d = std::make_unique<FilterDial>(find(t[i]), find(f[i]), find(r[i]), data->filterDials[i].ring, data->filterDials[i].curve, data->filterDials[i].rect, "Filter " + juce::String((int) i + 1)); addAndMakeVisible(*d); filterDials.push_back(std::move(d)); }
        }
    }
    // the little waveform in the menu rows follows the waveform and pulse width of its oscillator
    juce::String iconSignature() const
    {
        static const char* w[3] = { "cc_19", "cc_29", "cc_41" }; static const char* pw[3] = { "cc_21", "cc_31", "cc_43" }; juce::String sig;
        for (const char* m : { "cc_51", "cc_52", "cc_53", "cc_54", "cc_55", "cc_56" }) if (auto* q = find(m)) sig << juce::roundToInt(pget(q)) << "|";
        for (int i = 0; i < 3; ++i) { if (auto* a = find(w[i])) sig << juce::roundToInt(a->convertFrom0to1(a->getValue())) << ","; if (auto* b = find(pw[i])) sig << juce::roundToInt(b->convertFrom0to1(b->getValue())) << ";"; }
        return sig;
    }
    void paintIcons(juce::Graphics& g)
    {
        static const char* w[3] = { "cc_19", "cc_29", "cc_41" }; static const char* pw[3] = { "cc_21", "cc_31", "cc_43" };
        if (!data->navBars.empty())
        {
            float bottom = 0; for (auto& r : data->navBars) bottom = juce::jmax(bottom, r.getBottom()); static const char* mix[6] = { "cc_51", "cc_52", "cc_53", "cc_54", "cc_55", "cc_56" };
            for (size_t i = 0; i < data->navBars.size() && i < 6; ++i) { auto* q = find(mix[i]); const float v = pget(q), h = 3.0f + v / 127.0f * 11.0f; g.setColour(v > 0 ? juce::Colours::white : juce::Colour(0xff555555)); g.fillRect(data->navBars[i].getX(), bottom - h, data->navBars[i].getWidth(), h); }
        }
        for (auto& ic : data->icons)
        {
            if (ic.n < 1 || ic.n > 3) continue; auto* a = find(w[ic.n - 1]); auto* b = find(pw[ic.n - 1]);
            const int idx = a ? juce::jlimit(0, waveCount() - 1, juce::roundToInt(a->convertFrom0to1(a->getValue()))) : 2; const float amt = b ? b->getValue() : -1.0f;
            g.setColour(ic.color); g.strokePath(wavePath(idx, ic.rect.reduced(2, 2), amt), juce::PathStrokeType(1.6f));
        }
    }
    void timerCallback() override
    {
        for (auto& f : fields) f->refresh(); for (auto& f : faders) f->refresh(); for (auto& b : bars) b->refresh(); if (envGraph) envGraph->refresh(); for (auto& f : filterDials) f->refresh(); for (auto& d : dropdowns) d->refresh(); for (auto& l : lfoViews) l->refresh(); if (routeView) routeView->refresh(); if (matrixView) matrixView->refresh(); if (fxView) fxView->refresh(); for (auto& b : bitSwitches) b->refresh();
        if (data) { auto sg = iconSignature(); if (sg != iconSig) { iconSig = sg; repaint(0, 0, 340, kH); } }
        for (auto& [i, fn] : live) { auto t = fn(); if (shown[i] != t) { shown[i] = t; repaint(data->runs[(size_t) i].r.expanded(8, 4).toNearestInt()); } }
    }
    void openPicker(WaveDial& wd)
    {
        picker = std::make_unique<WavePicker>(wd.current(), wd.colour); auto* dp = &wd;
        picker->onPick = [this, dp](int i) { dp->choose(i); closePicker(); };
        addAndMakeVisible(*picker); picker->setTopLeftPosition(juce::jlimit(8, kW - picker->getWidth() - 8, wd.getBounds().getCentreX() - picker->getWidth() / 2), juce::jlimit(8, kH - picker->getHeight() - 8, wd.getBottom() - 20)); picker->toFront(true);
    }
    void closePicker() { picker.reset(); }
    juce::String currentPage() const { return page; }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff0B0B0C));
        if (data && data->art.isValid()) { g.setImageResamplingQuality(juce::Graphics::highResamplingQuality); g.drawImage(data->art, 0, 0, kW, kH, 0, 0, data->art.getWidth(), data->art.getHeight()); }
        if (data) for (size_t ri = 0; ri < data->runs.size(); ++ri)
        {
            auto& r = data->runs[ri]; if (skip.count((int) ri) || r.t.startsWith("14 types:")) continue; auto lv = live.find((int) ri); const juce::String txt = lv != live.end() ? lv->second() : r.t;
            g.setColour(r.col); g.setFont(r.font);
            if (lv != live.end()) { const float w = textWidth(r.font, txt), mw = r.r.getWidth() + 6.0f; if (w > mw) g.setFont(r.font.withHeight(r.font.getHeight() * mw / w)); }
            if (r.rot % 180 != 0) { const auto c = r.r.getCentre(); g.saveState(); g.addTransform(juce::AffineTransform::rotation(juce::degreesToRadians((float) r.rot), c.x, c.y));
                g.drawText(txt, juce::Rectangle<float>(r.r.getHeight(), r.r.getWidth()).withCentre(c), juce::Justification::centred, false); g.restoreState(); }
            else g.drawText(txt, r.r.expanded(10, 0), juce::Justification::centred, false);
        }
        if (data) paintIcons(g);
        paintWood(g, { 0, 0, kCheek, kH }, true); paintWood(g, { kW - kCheek, 0, kCheek, kH }, false);   // the PurrOne wood, drawn over the cheeks baked into the artwork
    }
    // The wood of the PurrOne mockup: fine vertical grain, dark at both edges and lit in the middle like a rounded cheek.
    // The 35 colours are measured from the approved PurrOne mockup (left cheek, outer edge first).
    static void paintWood(juce::Graphics& g, juce::Rectangle<int> r, bool left)
    {
        static const uint32_t col[35] = { 0x2A1B0F, 0x372414, 0x3D2616, 0x3C2615, 0x462D19, 0x4B2F1C, 0x4F331D, 0x452E1A, 0x563720, 0x593A21, 0x5D3D23, 0x52361F, 0x674428, 0x6A4629, 0x634126, 0x6B4629, 0x674428, 0x664127, 0x533520, 0x5D3D22, 0x5A3A21, 0x563820, 0x462D1A, 0x51341E, 0x462C19, 0x422A18, 0x442B18, 0x3F2716, 0x392414, 0x2B1B10, 0x2C1C0F, 0x28190E, 0x24160C, 0x1B1109, 0x1D120A };
        for (int i = 0; i < r.getWidth(); ++i)
        {
            const int k = juce::jlimit(0, 34, i * 35 / juce::jmax(1, r.getWidth()));
            g.setColour(juce::Colour(0xff000000u | col[left ? k : 34 - k])); g.fillRect(r.getX() + i, r.getY(), 1, r.getHeight());
        }
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (picker) { closePicker(); return; }
        if (!data) return; const auto p = e.position;
        for (auto& n : data->nav) if (n.rect.contains(p) && n.page.isNotEmpty()) { showPage(n.page); return; }
        for (auto& b : data->buttons) if (b.rect.contains(p))
        {
            if (b.label == "Settings") { if (onAll) onAll(); }
            else if (b.label.isNotEmpty()) { if (onInfo) onInfo(); }
            else if (b.text.contains("100")) { if (onZoom) onZoom(); }
            return;
        }
    }
    void mouseDoubleClick(const juce::MouseEvent& e) override { if (e.position.x < 34 || e.position.x > kW - 34) if (onReset100) onReset100(); }
    void mouseMove(const juce::MouseEvent& e) override
    {
        bool hand = false; if (data) { for (auto& n : data->nav) hand |= n.rect.contains(e.position); for (auto& b : data->buttons) hand |= b.rect.contains(e.position); }
        setMouseCursor(hand ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }
private:
    juce::AudioProcessor& proc; KnobLook look; juce::String page; std::unique_ptr<PageData> data; std::vector<std::unique_ptr<Knob>> knobs; std::vector<std::unique_ptr<WaveDial>> dials; std::unique_ptr<WavePicker> picker;
    std::vector<std::unique_ptr<Field>> fields; std::vector<std::unique_ptr<Fader>> faders; std::vector<std::unique_ptr<Bar>> bars; std::unique_ptr<EnvGraph> envGraph; std::vector<std::unique_ptr<FilterDial>> filterDials; std::vector<std::unique_ptr<Dropdown>> dropdowns; std::vector<std::unique_ptr<LfoWindow>> lfoViews; std::unique_ptr<RouteTiles> routeView; std::unique_ptr<MatrixRows> matrixView; std::unique_ptr<FxEditor> fxView; int fxSel = 0; std::vector<std::unique_ptr<BitSwitch>> bitSwitches; std::map<int, std::function<juce::String()>> live; std::set<int> skip; std::map<int, juce::String> shown; juce::String iconSig;
};

class Editor : public juce::AudioProcessorEditor
{
public:
    std::function<void()> openAllParameters;
    std::function<void(int)> onWidth;   // told the window width whenever the user resizes (the plugin keeps it in its saved state)
    explicit Editor(juce::AudioProcessor& p) : AudioProcessorEditor(p), content(p)
    {
        tooltip = std::make_unique<juce::TooltipWindow>(this, 700);
        addAndMakeVisible(content);
        content.onZoom = [this] { cycleZoom(); }; content.onReset100 = [this] { setSize(kDefaultW, kH); };
        content.onAll = [this] { if (openAllParameters) openAllParameters(); };
        content.onInfo = [this] { juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "UltraNova Controller",
            "Free and open source by Studio 87. Controls the Novation UltraNova over MIDI (CC and NRPN from the maker's list). It collects nothing and uses no network. "
            "Knobs drawn dim have no controller in the maker's list yet. The gear button opens the list of all parameters. Support: ko-fi.com/studio87"); };
        const char* pg = std::getenv("UNOVA_PAGE"); content.showPage(pg ? pg : "osc");
        constrainer.setFixedAspectRatio((double) kW / kH); constrainer.setSizeLimits(674, 380, 2696, 1520); setConstrainer(&constrainer);
        border = std::make_unique<juce::ResizableBorderComponent>(this, &constrainer); border->setBorderThickness(juce::BorderSize<int>(10)); addAndMakeVisible(*border);
        setResizable(true, false); setSize(kDefaultW, kH);
    }
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xff0B0B0C)); }
    void resized() override
    {
        content.setTransform(juce::AffineTransform::scale((float) getWidth() / (float) kW)); content.setBounds(0, 0, kW, kH);
        if (border) { border->setBounds(getLocalBounds()); border->toFront(false); }
        if (onWidth && getWidth() > 0) onWidth(getWidth());
    }
    void restoreWidth(int w) { if (w < 674) return; w = juce::jmin(w, 2696); setSize(w, (int) std::lround(w * (double) kH / kW)); }
private:
    void cycleZoom()
    {
        const int steps[] = { 50, 70, 85, 100, 115, 130, 150 }; const int cur = juce::roundToInt((float) getWidth() / kDefaultW * 100.0f); int next = steps[0];
        for (int v : steps) if (v > cur + 1) { next = v; break; }
        const int w = kDefaultW * next / 100; setSize(w, (int) std::lround(w * (double) kH / kW));
    }
    Content content; juce::ComponentBoundsConstrainer constrainer; std::unique_ptr<juce::ResizableBorderComponent> border; std::unique_ptr<juce::TooltipWindow> tooltip;
};
}
