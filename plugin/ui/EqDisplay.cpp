#include "EqDisplay.h"
#include "Parameters.h"

namespace ui {
namespace {
constexpr float kMinHz = 20.0f, kMaxHz = 20000.0f, kRangeDb = 18.0f;
juce::String freqText(float hz) { return hz >= 1000.0f ? juce::String(hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz" : juce::String(juce::roundToInt(hz)) + " Hz"; }
}

EqDisplay::EqDisplay(juce::AudioProcessorValueTreeState& state) : state_(state) {
  for (int b = 0; b < 6; ++b) {
    auto make = [&](const char* what) {
      return std::make_unique<juce::ParameterAttachment>(*state.getParameter(ei::id::eq(b + 1, what)), [](float) {});
    };
    bands_[size_t(b)].on = make("On"); bands_[size_t(b)].freq = make("Freq"); bands_[size_t(b)].gain = make("Gain"); bands_[size_t(b)].q = make("Q");
  }
  model_.prepare(48000.0);
  refresh();
}

void EqDisplay::refresh() {
  juce::uint32 sig = 17;
  for (int b = 0; b < 6; ++b) {
    auto raw = [&](const char* what) { return state_.getRawParameterValue(ei::id::eq(b + 1, what))->load(); };
    ei::EqBand v; v.on = raw("On") > 0.5f; v.freq = raw("Freq"); v.gainDb = raw("Gain"); v.q = raw("Q");
    bands_[size_t(b)].value = v;
    model_.setBand(b, v);
    sig = sig * 31 + juce::uint32(v.on) + juce::uint32(v.freq * 10.0f) * 7 + juce::uint32((v.gainDb + 20.0f) * 100.0f) * 13 + juce::uint32(v.q * 100.0f) * 3;
  }
  highCut_ = state_.getRawParameterValue(ei::id::eq6HighCut)->load() > 0.5f;
  model_.setBand6HighCut(highCut_);
  sig = sig * 31 + juce::uint32(highCut_);
  if (sig != signature_) { signature_ = sig; repaint(); }
}

juce::Rectangle<float> EqDisplay::plot() const { return getLocalBounds().toFloat().withTrimmedTop(34).withTrimmedBottom(30).reduced(14, 0); }
float EqDisplay::freqToX(float hz) const { const auto p = plot(); return p.getX() + p.getWidth() * std::log10(juce::jlimit(kMinHz, kMaxHz, hz) / kMinHz) / 3.0f; }
float EqDisplay::xToFreq(float x) const { const auto p = plot(); return kMinHz * std::pow(1000.0f, juce::jlimit(0.0f, 1.0f, (x - p.getX()) / p.getWidth())); }
float EqDisplay::dbToY(float db) const { const auto p = plot(); return p.getCentreY() - juce::jlimit(-kRangeDb, kRangeDb, db) / kRangeDb * p.getHeight() * 0.5f; }
float EqDisplay::yToDb(float y) const { const auto p = plot(); return juce::jlimit(-kRangeDb, kRangeDb, (p.getCentreY() - y) / (p.getHeight() * 0.5f) * kRangeDb); }
bool EqDisplay::isCut(int band) const { return band == 0 || (band == 5 && highCut_); }

juce::Point<float> EqDisplay::nodePos(int band) const {
  const ei::EqBand& v = bands_[size_t(band)].value;
  return {freqToX(v.freq), dbToY(isCut(band) ? -3.0f : v.gainDb)};
}
int EqDisplay::bandAt(juce::Point<float> p) const {
  int best = -1; float bestDist = 14.0f;
  for (int b = 0; b < 6; ++b) { const float d = nodePos(b).getDistanceFrom(p); if (d < bestDist) { bestDist = d; best = b; } }
  return best;
}

void EqDisplay::mouseMove(const juce::MouseEvent& e) {
  const int b = bandAt(e.position);
  if (b != hover_) { hover_ = b; repaint(); }
  setMouseCursor(b < 0 ? juce::MouseCursor::NormalCursor : juce::MouseCursor::DraggingHandCursor);
}
void EqDisplay::mouseDown(const juce::MouseEvent& e) {
  drag_ = bandAt(e.position);
  if (drag_ < 0) return;
  selected_ = drag_;
  bands_[size_t(drag_)].freq->beginGesture(); bands_[size_t(drag_)].gain->beginGesture();
  repaint();
}
void EqDisplay::mouseDrag(const juce::MouseEvent& e) {
  if (drag_ < 0) return;
  bands_[size_t(drag_)].freq->setValueAsPartOfGesture(xToFreq(e.position.x));
  if (!isCut(drag_)) bands_[size_t(drag_)].gain->setValueAsPartOfGesture(yToDb(e.position.y));
  refresh();
}
void EqDisplay::mouseUp(const juce::MouseEvent&) {
  if (drag_ < 0) return;
  bands_[size_t(drag_)].freq->endGesture(); bands_[size_t(drag_)].gain->endGesture();
  drag_ = -1;
}
void EqDisplay::mouseDoubleClick(const juce::MouseEvent& e) {
  const int b = bandAt(e.position);
  if (b < 0) return;
  bands_[size_t(b)].on->setValueAsCompleteGesture(bands_[size_t(b)].value.on ? 0.0f : 1.0f);
  refresh();
}
void EqDisplay::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
  int b = bandAt(e.position);
  if (b < 0) b = selected_;
  const float q = juce::jlimit(0.1f, 10.0f, bands_[size_t(b)].value.q * std::pow(2.0f, wheel.deltaY * 2.0f));
  bands_[size_t(b)].q->setValueAsCompleteGesture(q);
  selected_ = b;
  refresh();
}

void EqDisplay::paint(juce::Graphics& g) {
  const auto bounds = getLocalBounds().toFloat();
  drawFrame(g, bounds, juce::String::fromUTF8("EQ · 6 BANDS"));
  const auto p = plot();

  g.setFont(dataFont(10.0f));
  const std::pair<float, const char*> ticks[] = {{20.0f, "20"}, {100.0f, "100"}, {1000.0f, "1k"}, {10000.0f, "10k"}};
  for (auto& t : ticks) {
    g.setColour(col::line); g.drawVerticalLine(juce::roundToInt(freqToX(t.first)), p.getY(), p.getBottom());
    g.setColour(col::dim); g.drawText(t.second, juce::Rectangle<float>(freqToX(t.first) + 3, p.getBottom() + 3, 40, 12).toNearestInt(), juce::Justification::centredLeft);
  }
  for (float db : {12.0f, 0.0f, -12.0f}) {
    const float y = dbToY(db);
    if (db == 0.0f) { g.setColour(col::dim.withAlpha(0.7f)); g.drawHorizontalLine(juce::roundToInt(y), p.getX(), p.getRight()); }
    else { const float dashes[] = {1.0f, 5.0f}; g.setColour(col::line); g.drawDashedLine({p.getX(), y, p.getRight(), y}, dashes, 2, 1.0f); }
    g.setColour(col::dim);
    g.drawText((db > 0 ? "+" : "") + juce::String(int(db)), juce::Rectangle<float>(p.getRight() - 42, y - 13, 40, 12).toNearestInt(), juce::Justification::centredRight);
  }

  // Response curve.
  juce::Path curve;
  const int steps = 160;
  for (int i = 0; i <= steps; ++i) {
    const float f = kMinHz * std::pow(1000.0f, float(i) / float(steps));
    const float y = dbToY(model_.magnitudeDb(f));
    if (i == 0) curve.startNewSubPath(freqToX(f), y); else curve.lineTo(freqToX(f), y);
  }
  {
    juce::Graphics::ScopedSaveState clip(g);
    g.reduceClipRegion(p.toNearestInt());
    juce::Path fill(curve); fill.lineTo(p.getRight(), p.getBottom()); fill.lineTo(p.getX(), p.getBottom()); fill.closeSubPath();
    g.setGradientFill(juce::ColourGradient(col::green.withAlpha(0.22f), 0, p.getY(), col::green.withAlpha(0.0f), 0, p.getBottom(), false));
    g.fillPath(fill);
    glowStroke(g, curve, col::green, 2.0f);
  }

  // Nodes.
  for (int b = 0; b < 6; ++b) {
    const auto c = nodePos(b);
    const bool on = bands_[size_t(b)].value.on, sel = b == selected_;
    const float r = (sel || b == hover_) ? 8.0f : 6.5f;
    const auto circle = juce::Rectangle<float>(2 * r, 2 * r).withCentre(c);
    g.setColour(col::voidBlack); g.fillEllipse(circle);
    if (sel && on) { g.setColour(col::mint.withAlpha(0.2f)); g.drawEllipse(circle.expanded(2.5f), 3.0f); }
    g.setColour(!on ? col::dim.withAlpha(0.6f) : (sel ? col::mint : col::green)); g.drawEllipse(circle, 1.5f);
    g.setColour(on ? col::mint : col::dim); g.setFont(dataFont(10.0f));
    g.drawText(juce::String(b + 1), circle.toNearestInt(), juce::Justification::centred);
  }

  // Read-out for the selected band.
  const ei::EqBand& v = bands_[size_t(selected_)].value;
  const char* type = selected_ == 0 ? "LOW CUT" : selected_ == 1 ? "LOW SHELF" : selected_ == 5 ? (highCut_ ? "HIGH CUT" : "HIGH SHELF") : "BELL";
  juce::String text = juce::String(selected_ + 1) + " " + type + "  " + freqText(v.freq);
  if (!isCut(selected_)) text << "  " << (v.gainDb >= 0 ? "+" : "") << juce::String(v.gainDb, 1) << " dB";
  text << "  Q " << juce::String(v.q, 2);
  if (!v.on) text << "  OFF";
  g.setColour(v.on ? col::mint : col::dim); g.setFont(dataFont(10.0f));
  g.drawText(text, bounds.withHeight(26).withTrimmedRight(12).withTrimmedLeft(110).toNearestInt(), juce::Justification::centredRight);
}

}  // namespace ui
