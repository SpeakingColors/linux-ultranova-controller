// The real UltraNova Controller window, built from the approved mockup (docs/mockups).
// tools/export_mockup.py turns every mockup page into artwork (assets/ui/<page>.png, 2x) plus a list of where
// the live controls sit (assets/ui/<page>.json). This file draws the artwork and puts real controls on top.
// Controls that are not wired to a parameter yet are drawn disabled and listed in kKnobMap with an empty id.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <map>
#include <memory>
#include <vector>
#include "BinaryData.h"

namespace unova
{
constexpr int kW = 1348, kH = 760;          // design size, the same for every page
constexpr int kDefaultW = 1348;             // 100%

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
                   "", "cc_59",                                                            // effects send (Pre-FX level, CC number unclear in the source), output (Post-FX level)
                   "cc_15", "cc_5",                                                        // unison detune, portamento time
                   "cc_108", "cc_111", "cc_114" } },                                       // amp envelope: velocity, repeat, level track
        { "filter", { "cc_61",
                      "cc_69", "cc_79", "cc_74", "cc_71", "cc_78", "cc_63",                // filter 1: keytrack, env amount, frequency, resonance, q normalise, drive
                      "cc_84", "cc_87", "cc_83", "cc_85", "cc_86", "cc_80",                // filter 2
                      "nrpn_0_0", "nrpn_0_7", "nrpn_0_10" } },                             // filter envelope: velocity, repeat, level track
        { "mod", { "nrpn_0_76", "nrpn_0_72", "nrpn_0_74",                                  // LFO 1: rate, slew, delay
                   "nrpn_0_56", "nrpn_0_63", "nrpn_0_66" } },                              // Envelope 6: delay, repeat, level track
        { "fx", { "nrpn_0_105", "nrpn_0_107", "nrpn_0_109", "nrpn_0_104", "nrpn_0_106", "nrpn_0_108",   // EQ levels and frequencies
                  "nrpn_0_98", "",                                                          // feedback, FX wet level (no clear CC)
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

struct KnobSpec { juce::Rectangle<float> rect; float angle = 0; };
struct NavSpec { juce::String page; juce::Rectangle<float> rect; };
struct ButtonSpec { juce::String text, label; juce::Rectangle<float> rect; };
struct PageData
{
    juce::Image art; std::vector<KnobSpec> knobs; std::vector<NavSpec> nav; std::vector<ButtonSpec> buttons;
    static std::unique_ptr<PageData> load(const juce::String& key)
    {
        auto pd = std::make_unique<PageData>(); auto png = resource(key + ".png"); auto js = resource(key + ".json");
        pd->art = juce::ImageFileFormat::loadFrom(png.getData(), png.getSize());
        auto v = juce::JSON::parse(juce::String::fromUTF8((const char*) js.getData(), (int) js.getSize()));
        auto rectOf = [](const juce::var& r) { return juce::Rectangle<float>((float) (double) r["x"], (float) (double) r["y"], (float) (double) r["w"], (float) (double) r["h"]); };
        if (auto* ks = v["knobs"].getArray()) for (auto& k : *ks) { KnobSpec s; s.rect = rectOf(k["rect"]); s.angle = k["angle"].isVoid() ? -135.0f : (float) (double) k["angle"]; pd->knobs.push_back(s); }
        if (auto* ns = v["nav"].getArray()) for (auto& n : *ns) pd->nav.push_back({ pageKeyFromHref(n["page"].toString()), rectOf(n["rect"]) });
        if (auto* bs = v["buttons"].getArray()) for (auto& b : *bs) pd->buttons.push_back({ b["text"].toString(), b["label"].toString(), rectOf(b["rect"]) });
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
class Content : public juce::Component
{
public:
    std::function<void()> onZoom, onInfo, onAll, onReset100;
    Content(juce::AudioProcessor& p) : proc(p) { setSize(kW, kH); setOpaque(true); }
    ~Content() override { knobs.clear(); }

    void showPage(const juce::String& key)
    {
        knobs.clear(); page = key; data = PageData::load(key); const auto& ids = knobMap().at(key);
        for (size_t i = 0; i < data->knobs.size(); ++i)
        {
            juce::RangedAudioParameter* prm = nullptr;
            if (i < ids.size() && ids[i].isNotEmpty())
                for (auto* q : proc.getParameters()) if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(q)) if (r->paramID == ids[i]) { prm = r; break; }
            auto k = std::make_unique<Knob>(prm, data->knobs[i].angle, look); k->setBounds(data->knobs[i].rect.toNearestInt()); addAndMakeVisible(*k); knobs.push_back(std::move(k));
        }
        repaint();
    }
    juce::String currentPage() const { return page; }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff0B0B0C));
        if (data && data->art.isValid()) { g.setImageResamplingQuality(juce::Graphics::highResamplingQuality); g.drawImage(data->art, 0, 0, kW, kH, 0, 0, data->art.getWidth(), data->art.getHeight()); }
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
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
    juce::AudioProcessor& proc; KnobLook look; juce::String page; std::unique_ptr<PageData> data; std::vector<std::unique_ptr<Knob>> knobs;
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
