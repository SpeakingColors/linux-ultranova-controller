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
struct IconSpec { int n = 1; juce::Rectangle<float> rect; juce::Colour color; };
struct DialSpec { juce::Rectangle<float> rect; juce::Colour color; };
struct KnobSpec { juce::Rectangle<float> rect; float angle = 0; };
struct NavSpec { juce::String page; juce::Rectangle<float> rect; };
struct ButtonSpec { juce::String text, label; juce::Rectangle<float> rect; };
struct PageData
{
    juce::Image art; std::vector<KnobSpec> knobs; std::vector<NavSpec> nav; std::vector<ButtonSpec> buttons; std::vector<TextRun> runs; std::vector<DialSpec> dials; std::vector<IconSpec> icons;
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

// A knob wired to one parameter (or disabled at the mockup's resting angle when there is none).
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
    ~Content() override { stopTimer(); knobs.clear(); fields.clear(); }

    void showPage(const juce::String& key)
    {
        knobs.clear(); dials.clear(); fields.clear(); live.clear(); skip.clear(); shown.clear(); picker.reset(); page = key; data = PageData::load(key); const auto& ids = knobMap().at(key);
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
        static const char* semi[3] = { "cc_26", "cc_37", "cc_48" }; static const char* cents[3] = { "cc_27", "cc_39", "cc_49" }; int ns = 0, nc = 0;
        for (size_t i = 0; i < data->runs.size(); ++i)
        {
            const auto& r = data->runs[i]; if (r.r.getX() > 330) continue;
            if (r.t.endsWith(" Semi") && ns < 3) { if (auto* p = find(semi[ns++])) live[(int) i] = [p] { return Field::text(p, true) + " Semi"; }; }
            else if (r.t.endsWith(" Detune") && nc < 3) { if (auto* p = find(cents[nc++])) live[(int) i] = [p] { return Field::text(p, true) + " Detune"; }; }
        }
        if (key != "osc") return;
        auto box = [this](int run, const char* id, bool centred) {
            auto* p = find(id); if (!p) return; const auto& r = data->runs[(size_t) run]; skip.insert(run);
            auto f = std::make_unique<Field>(p, centred, r, juce::Rectangle<float>(40, 20).withCentre(r.r.getCentre())); addAndMakeVisible(*f); fields.push_back(std::move(f)); };
        box(78, "cc_26", true); box(79, "cc_28", true); box(104, "cc_37", true); box(105, "cc_40", true); box(130, "cc_48", true); box(131, "cc_50", true);
        box(171, "cc_14", false);
    }
    // the little waveform in the menu rows follows the waveform and pulse width of its oscillator
    juce::String iconSignature() const
    {
        static const char* w[3] = { "cc_19", "cc_29", "cc_41" }; static const char* pw[3] = { "cc_21", "cc_31", "cc_43" }; juce::String sig;
        for (int i = 0; i < 3; ++i) { if (auto* a = find(w[i])) sig << juce::roundToInt(a->convertFrom0to1(a->getValue())) << ","; if (auto* b = find(pw[i])) sig << juce::roundToInt(b->convertFrom0to1(b->getValue())) << ";"; }
        return sig;
    }
    void paintIcons(juce::Graphics& g)
    {
        static const char* w[3] = { "cc_19", "cc_29", "cc_41" }; static const char* pw[3] = { "cc_21", "cc_31", "cc_43" };
        for (auto& ic : data->icons)
        {
            if (ic.n < 1 || ic.n > 3) continue; auto* a = find(w[ic.n - 1]); auto* b = find(pw[ic.n - 1]);
            const int idx = a ? juce::jlimit(0, waveCount() - 1, juce::roundToInt(a->convertFrom0to1(a->getValue()))) : 2; const float amt = b ? b->getValue() : -1.0f;
            g.setColour(ic.color); g.strokePath(wavePath(idx, ic.rect.reduced(2, 2), amt), juce::PathStrokeType(1.6f));
        }
    }
    void timerCallback() override
    {
        for (auto& f : fields) f->refresh();
        if (data) { auto sg = iconSignature(); if (sg != iconSig) { iconSig = sg; for (auto& ic : data->icons) repaint(ic.rect.expanded(3).toNearestInt()); } }
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
            auto& r = data->runs[ri]; if (skip.count((int) ri)) continue; auto lv = live.find((int) ri); const juce::String txt = lv != live.end() ? lv->second() : r.t;
            g.setColour(r.col); g.setFont(r.font);
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
    std::vector<std::unique_ptr<Field>> fields; std::map<int, std::function<juce::String()>> live; std::set<int> skip; std::map<int, juce::String> shown; juce::String iconSig;
};

class Editor : public juce::AudioProcessorEditor
{
public:
    std::function<void()> openAllParameters;
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
    }
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
