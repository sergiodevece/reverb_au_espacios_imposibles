#include "PluginEditor.h"

namespace {
using ui::col::dim;
// Layout, in pixels, shared by paint() and resized().
const juce::Rectangle<int> kEnvelope{16, 60, 624, 300}, kEq{652, 60, 332, 300};
constexpr int kRowY = 372, kRowH = 252;
const juce::Rectangle<int> kSpace{16, kRowY, 314, kRowH}, kLfo{340, kRowY, 220, kRowH}, kReverse{570, kRowY, 120, kRowH},
    kGate{700, kRowY, 170, kRowH}, kOutput{880, kRowY, 104, kRowH};
constexpr float kRow1 = float(kRowY) + 78.0f, kRow2 = float(kRowY) + 178.0f;
const int kUiHz = 30;
}  // namespace

ImpossibleSpacesEditor::ImpossibleSpacesEditor(ImpossibleSpacesProcessor& p)
    : AudioProcessorEditor(&p), processor_(p), state_(p.apvts),
      envelope_(state_), eq_(state_), eqBand6Mode_(state_, ei::id::eq6HighCut, {"SHELF", "CUT"}),
      size_(state_, ei::id::size, "SIZE"), diffusion_(state_, ei::id::diffusion, "DIFFUSION"),
      damping_(state_, ei::id::damping, "DAMPING", true), width_(state_, ei::id::width, "WIDTH"),
      pitch_(state_, ei::id::pitch, "PITCH", true), pitchMix_(state_, ei::id::pitchMix, "PITCH MIX"),
      freeze_(state_, ei::id::freeze, ui::Toggle::Style::Box, "FREEZE"),
      lfoOn_(state_, ei::id::lfoOn, ui::Toggle::Style::Led),
      lfoMode_(state_, ei::id::lfoMode, {"AUTO", "MANUAL"}),
      lfoShape_(state_, ei::id::lfoShape, {"SIN", "TRI", "SAW", "SQR", "RND"}),
      lfoTarget_(state_, ei::id::lfoTarget, {"SIZE", "PITCH", "DAMP", "PAN"}),
      lfoRate_(state_, ei::id::lfoRate, "RATE"), lfoDivision_(state_, ei::id::lfoDivision, "RATE"),
      lfoDepth_(state_, ei::id::lfoDepth, "DEPTH"), lfoPhase_(state_, ei::id::lfoPhase, "PHASE"),
      lfoSync_(state_, ei::id::lfoSync, ui::Toggle::Style::Box, "SYNC"),
      revOn_(state_, ei::id::revOn, ui::Toggle::Style::Led),
      revWindow_(state_, ei::id::revWindow, "WINDOW"), revDivision_(state_, ei::id::revDivision, "WINDOW"),
      revSync_(state_, ei::id::revSync, {"FREE", "SYNC"}),
      gateOn_(state_, ei::id::gateOn, ui::Toggle::Style::Led),
      gateThreshold_(state_, ei::id::gateThreshold, "THRESHOLD"), gateRange_(state_, ei::id::gateRange, "RANGE"),
      gateAttack_(state_, ei::id::gateAttack, "ATTACK"), gateHold_(state_, ei::id::gateHold, "HOLD"),
      gateRelease_(state_, ei::id::gateRelease, "RELEASE"),
      gateSource_(state_, ei::id::gateSource, {"DRY", "WET"}),
      mix_(state_, ei::id::mix, "MIX"), output_(state_, ei::id::output, "LEVEL", true) {
  for (juce::Component* c : std::initializer_list<juce::Component*>{
           &envelope_, &eq_, &eqBand6Mode_, &size_, &diffusion_, &damping_, &width_, &pitch_, &pitchMix_, &freeze_,
           &lfoOn_, &lfoMode_, &lfoShape_, &lfoTarget_, &lfoView_, &lfoRate_, &lfoDivision_, &lfoDepth_, &lfoPhase_, &lfoSync_,
           &revOn_, &revWindow_, &revDivision_, &revSync_,
           &gateOn_, &gateThreshold_, &gateRange_, &gateAttack_, &gateHold_, &gateRelease_, &gateSource_, &mix_, &output_})
    addAndMakeVisible(c);
  setResizable(false, false);
  setSize(kWidth, kHeight);
  timerCallback();
  startTimerHz(kUiHz);
}

float ImpossibleSpacesEditor::value(const char* paramId) const { return state_.getRawParameterValue(paramId)->load(); }

void ImpossibleSpacesEditor::resized() {
  envelope_.setBounds(kEnvelope);
  eq_.setBounds(kEq);
  eqBand6Mode_.setBounds(kEq.getX() + 208, kEq.getBottom() - 23, 78, 15);   // sits under band 6, between the 1k and 10k labels

  // Space
  const float sx[4] = {kSpace.getX() + 46.0f, kSpace.getX() + 120.0f, kSpace.getX() + 194.0f, kSpace.getX() + 268.0f};
  size_.place(sx[0], kRow1, 20); diffusion_.place(sx[1], kRow1, 20); damping_.place(sx[2], kRow1, 20); width_.place(sx[3], kRow1, 20);
  pitch_.place(sx[0], kRow2, 20); pitchMix_.place(sx[1], kRow2, 20);
  freeze_.setBounds(int(sx[2]) - 26, int(kRow2) - 18, 126, 36);

  // LFO
  const int lx = kLfo.getX(), lw = kLfo.getWidth();
  lfoOn_.setBounds(lx + 44, kRowY + 5, 44, 16);
  lfoMode_.setBounds(lx + lw - 112, kRowY + 6, 100, 15);
  lfoView_.setBounds(lx + 12, kRowY + 32, lw - 24, 48);
  lfoShape_.setBounds(lx + 60, kRowY + 88, lw - 72, 15);
  lfoTarget_.setBounds(lx + 60, kRowY + 108, lw - 72, 15);
  const float lcy = float(kRowY) + 178.0f;
  lfoRate_.place(lx + 40.0f, lcy, 18); lfoDivision_.place(lx + 40.0f, lcy, 18);
  lfoDepth_.place(lx + 110.0f, lcy, 18); lfoPhase_.place(lx + 180.0f, lcy, 18);
  lfoSync_.setBounds(lx + 20, kRowY + 226, 40, 15);

  // Reverse
  const int rx = kReverse.getX(), rw = kReverse.getWidth();
  revOn_.setBounds(rx + rw - 52, kRowY + 5, 44, 16);
  revWindow_.place(rx + rw * 0.5f, kRow1, 22); revDivision_.place(rx + rw * 0.5f, kRow1, 22);
  revSync_.setBounds(rx + 16, kRowY + 222, rw - 32, 15);

  // Gate
  const int gx = kGate.getX(), gw = kGate.getWidth();
  gateOn_.setBounds(gx + gw - 52, kRowY + 5, 44, 16);
  gateThreshold_.place(gx + 48.0f, kRow1, 20); gateRange_.place(gx + 122.0f, kRow1, 20);
  gateSource_.setBounds(gx + 60, kRowY + 126, gw - 72, 15);
  const float gcy = float(kRowY) + 196.0f;
  gateAttack_.place(gx + 34.0f, gcy, 15); gateHold_.place(gx + 85.0f, gcy, 15); gateRelease_.place(gx + 136.0f, gcy, 15);

  // Output
  mix_.place(kOutput.getCentreX(), kRow1, 24); output_.place(kOutput.getCentreX(), kRow2 + 6.0f, 18);
}

void ImpossibleSpacesEditor::paint(juce::Graphics& g) {
  g.fillAll(ui::col::voidBlack);

  // Penrose triangle: three bars that could never meet.
  {
    const juce::Point<float> c(34.0f, 27.0f);
    const float R = 15.0f, r = 6.5f;
    auto at = [&](float radius, float deg) { const float a = juce::degreesToRadians(deg - 90.0f); return c + juce::Point<float>(std::cos(a), std::sin(a)) * radius; };
    juce::Path tri;
    for (int i = 0; i < 3; ++i) {
      const float a = float(i) * 120.0f;
      const auto o1 = at(R, a), o2 = at(R, a + 120.0f), o3 = at(R, a + 240.0f);
      const auto m = o2 * 0.72f + o3 * 0.28f;
      tri.startNewSubPath(o1); tri.lineTo(o2); tri.lineTo(m); tri.lineTo(at(r, a + 120.0f)); tri.lineTo(at(r, a + 240.0f));
    }
    ui::glowStroke(g, tri, ui::col::green, 1.3f);
  }
  g.setColour(ui::col::mint); g.setFont(ui::displayFont(19.0f, 0.2f));
  g.drawText("IMPOSSIBLE SPACES", 60, 12, 420, 30, juce::Justification::centredLeft);
  g.setColour(dim); g.setFont(ui::labelFont(10.5f, false, 0.08f));
  g.drawText("designed by SerxDev", kWidth - 216, 10, 200, 14, juce::Justification::centredRight);
  g.setColour(ui::col::green.withAlpha(0.8f)); g.setFont(ui::displayFont(11.0f, 0.3f));
  g.drawText("VITAPLUG", kWidth - 216, 27, 203, 16, juce::Justification::centredRight);
  g.setColour(ui::col::line); g.fillRect(16, 50, kWidth - 32, 1);
  g.setColour(ui::col::green.withAlpha(0.7f)); g.fillRect(16, 50, 314, 1);

  ui::drawFrame(g, kSpace.toFloat(), "SPACE");
  ui::drawFrame(g, kLfo.toFloat(), "LFO");
  ui::drawFrame(g, kReverse.toFloat(), "REVERSE");
  ui::drawFrame(g, kGate.toFloat(), "GATE");
  ui::drawFrame(g, kOutput.toFloat(), "OUTPUT");

  g.setColour(dim); g.setFont(ui::labelFont(10.5f));
  g.drawText("SHAPE", kLfo.getX() + 12, kRowY + 88, 46, 15, juce::Justification::centredLeft);
  g.drawText("TARGET", kLfo.getX() + 12, kRowY + 108, 46, 15, juce::Justification::centredLeft);
  g.drawText("SOURCE", kGate.getX() + 12, kRowY + 126, 46, 15, juce::Justification::centredLeft);
  g.setFont(ui::dataFont(10.0f));
  g.drawText(juce::String::fromUTF8("∞ hold"), freeze_.getX(), freeze_.getBottom() + 6, freeze_.getWidth(), 12, juce::Justification::centred);

  // Reverse glyph: a tail that swells instead of fading.
  {
    const bool on = value(ei::id::revOn) > 0.5f;
    const auto b = juce::Rectangle<float>(kReverse.getX() + 16.0f, kRowY + 150.0f, kReverse.getWidth() - 32.0f, 44.0f);
    juce::Path swell; swell.startNewSubPath(b.getX(), b.getBottom());
    for (int i = 0; i <= 30; ++i) { const float t = float(i) / 30.0f; swell.lineTo(b.getX() + b.getWidth() * t, b.getBottom() - b.getHeight() * std::pow(t, 2.4f)); }
    swell.lineTo(b.getRight(), b.getBottom()); swell.closeSubPath();
    const auto c = on ? ui::col::green : dim.withAlpha(0.6f);
    g.setColour(c.withAlpha(on ? 0.2f : 0.08f)); g.fillPath(swell);
    g.setColour(c); g.strokePath(swell, juce::PathStrokeType(1.2f));
    g.setColour(dim); g.setFont(ui::labelFont(10.0f));
    g.drawText(juce::String::fromUTF8("◂ TIME"), b.translated(0, 48).withHeight(12).toNearestInt(), juce::Justification::centred);
  }
}

void ImpossibleSpacesEditor::timerCallback() {
  // Drain the audio thread's meter frames into one UI frame.
  ImpossibleSpacesProcessor::MeterFrame f;
  bool any = false; float in = 0, wet = 0;
  while (processor_.popMeterFrame(f)) { any = true; in = juce::jmax(in, f.in); wet = juce::jmax(wet, f.wet); lastGate_ = f.gateGain; lastLfo_ = f.lfo; }
  if (any) { heldIn_ = in; heldWet_ = wet; } else { heldIn_ *= 0.8f; heldWet_ *= 0.8f; }   // host stopped: let the picture fade
  envelope_.pushFrame(heldIn_, heldWet_, lastGate_, lastLfo_);
  eq_.refresh();

  const bool manual = value(ei::id::lfoMode) > 0.5f, lfoSync = value(ei::id::lfoSync) > 0.5f;
  const bool revOn = value(ei::id::revOn) > 0.5f, revSync = value(ei::id::revSync) > 0.5f, gateOn = value(ei::id::gateOn) > 0.5f;

  const bool lfoOn = value(ei::id::lfoOn) > 0.5f;
  lfoMode_.setActive(lfoOn); lfoTarget_.setActive(lfoOn); lfoDepth_.setActive(lfoOn); lfoDivision_.setActive(lfoOn);
  lfoView_.setAlpha(lfoOn ? 1.0f : 0.32f);
  lfoShape_.setActive(lfoOn && manual); lfoPhase_.setActive(lfoOn && manual); lfoSync_.setActive(lfoOn && manual);
  lfoRate_.setVisible(!(manual && lfoSync)); lfoDivision_.setVisible(manual && lfoSync);
  lfoRate_.setActive(lfoOn && manual);
  revWindow_.setVisible(!revSync); revDivision_.setVisible(revSync);
  revWindow_.setActive(revOn); revDivision_.setActive(revOn); revSync_.setActive(revOn);
  for (ui::Knob* k : {&gateThreshold_, &gateRange_, &gateAttack_, &gateHold_, &gateRelease_}) k->setActive(gateOn);
  gateSource_.setActive(gateOn);
  pitchMix_.setActive(true);

  const double bpm = processor_.currentBpm();
  const float hz = !manual ? float(bpm / 240.0)
                           : (lfoSync ? float(bpm / 60.0 / double(ei::Lfo::divisionToBeats(juce::roundToInt(value(ei::id::lfoDivision))))) : value(ei::id::lfoRate));
  lfoView_.update(manual ? juce::roundToInt(value(ei::id::lfoShape)) : 0, value(ei::id::lfoDepth) * 0.01f,
                  manual ? value(ei::id::lfoPhase) / 360.0f : 0.0f, lfoOn ? hz : 0.0f, 1.0f / float(kUiHz));
  if (revOn != lastRevOn_) { lastRevOn_ = revOn; repaint(kReverse); }
}
