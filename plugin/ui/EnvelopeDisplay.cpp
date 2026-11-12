#include "EnvelopeDisplay.h"
#include "Parameters.h"

namespace ui {

EnvelopeDisplay::EnvelopeDisplay(juce::AudioProcessorValueTreeState& state)
    : state_(state),
      pre_(*state.getParameter(ei::id::preDelay), [this](float) { repaint(); }),
      attack_(*state.getParameter(ei::id::attack), [this](float) { repaint(); }) {
  for (int n = 0; n < kPoints; ++n) {
    time_[n] = std::make_unique<juce::ParameterAttachment>(*state.getParameter(ei::id::tail(n + 1, "Time")), [this](float) { repaint(); });
    level_[n] = std::make_unique<juce::ParameterAttachment>(*state.getParameter(ei::id::tail(n + 1, "Level")), [this](float) { repaint(); });
  }
  inDb_.fill(kBottomDb); wetDb_.fill(kBottomDb);
}

float EnvelopeDisplay::value(const juce::String& paramId) const { return state_.getRawParameterValue(paramId)->load(); }

void EnvelopeDisplay::pushFrame(float inputPeak, float wetPeak, float gateGain, float lfoValue) {
  inDb_[size_t(head_)] = juce::jlimit(kBottomDb, kTopDb, juce::Decibels::gainToDecibels(inputPeak, kBottomDb));
  wetDb_[size_t(head_)] = juce::jlimit(kBottomDb, kTopDb, juce::Decibels::gainToDecibels(wetPeak, kBottomDb));
  head_ = (head_ + 1) % kHistory;
  gateGain_ = gateGain; lfo_ = lfoValue;
  drift_ += 0.004f;
  repaint();
}

juce::Rectangle<float> EnvelopeDisplay::plot() const { return getLocalBounds().toFloat().withTrimmedTop(44).withTrimmedBottom(30).reduced(14, 0); }
float EnvelopeDisplay::timeToX(float s) const {
  const auto p = plot();
  return p.getX() + p.getWidth() * std::log10(juce::jlimit(kMinTime, kMaxTime, s) / kMinTime) / std::log10(kMaxTime / kMinTime);
}
float EnvelopeDisplay::xToTime(float x) const {
  const auto p = plot();
  return kMinTime * std::pow(kMaxTime / kMinTime, juce::jlimit(0.0f, 1.0f, (x - p.getX()) / p.getWidth()));
}
// One level scale for everything on screen: +12 dB just under the top edge, -60 dB on the baseline.
float EnvelopeDisplay::levelToY(float db) const {
  const auto p = plot();
  const float top = p.getY() + 10.0f;
  return top + (p.getBottom() - top) * (kTopDb - juce::jlimit(kBottomDb, kTopDb, db)) / (kTopDb - kBottomDb);
}
float EnvelopeDisplay::yToLevel(float y) const {
  const auto p = plot();
  const float top = p.getY() + 10.0f;
  return juce::jlimit(kBottomDb, kTopDb, kTopDb - (y - top) / (p.getBottom() - top) * (kTopDb - kBottomDb));
}

EnvelopeDisplay::Shape EnvelopeDisplay::shape() const {
  const float pre = value(ei::id::preDelay) * 0.001f, att = value(ei::id::attack);
  Shape s;
  s.bottom = plot().getBottom(); s.yZero = levelToY(0.0f);
  s.xPre = timeToX(pre); s.xAttack = timeToX(pre + att);
  float previous = 0.0f, x = s.xAttack;
  for (int n = 0; n < kPoints; ++n) {
    s.time[n] = juce::jmax(previous, value(ei::id::tail(n + 1, "Time")));        // same ordering rule as the engine
    s.level[n] = value(ei::id::tail(n + 1, "Level"));
    x = juce::jmax(x, timeToX(s.time[n]));                                        // never drawn left of where the reverb comes in
    s.pt[n] = {x, levelToY(s.level[n])};
    previous = s.time[n];
  }
  s.neverDies = s.level[kPoints - 1] > kBottomDb + 1.0f;
  return s;
}

juce::Point<float> EnvelopeDisplay::handlePos(int h, const Shape& s) const {
  if (h == kPre) return {s.xPre, s.bottom};
  if (h == kAttack) return {0.5f * (s.xPre + s.xAttack), 0.5f * (s.bottom + s.yZero)};      // halfway up the rising edge
  if (h >= kFirstPoint && h < kNumHandles) return s.pt[h - kFirstPoint];
  return {};
}

int EnvelopeDisplay::handleAt(juce::Point<float> pt) const {
  const Shape s = shape();
  int best = kNone; float bestDist = 14.0f;
  for (int h = kNumHandles - 1; h >= 0; --h) {            // later points win ties, so stacked points can be pulled apart
    const float d = handlePos(h, s).getDistanceFrom(pt);
    if (d < bestDist) { bestDist = d; best = h; }
  }
  return best;
}

void EnvelopeDisplay::mouseMove(const juce::MouseEvent& e) {
  const int h = handleAt(e.position);
  if (h != hover_) { hover_ = h; repaint(); }
  setMouseCursor(h == kNone ? juce::MouseCursor::NormalCursor
                            : (h < kFirstPoint ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::DraggingHandCursor));
}

void EnvelopeDisplay::mouseDown(const juce::MouseEvent& e) {
  drag_ = handleAt(e.position);
  if (drag_ == kPre) pre_.beginGesture();
  else if (drag_ == kAttack) attack_.beginGesture();
  else if (drag_ >= kFirstPoint) { time_[drag_ - kFirstPoint]->beginGesture(); level_[drag_ - kFirstPoint]->beginGesture(); }
}

void EnvelopeDisplay::mouseDrag(const juce::MouseEvent& e) {
  if (drag_ == kNone) return;
  const auto p = plot();
  const Shape s = shape();
  auto timeAt = [&](float x) { return x <= p.getX() + 1.0f ? 0.0f : xToTime(x); };
  if (drag_ == kPre) {
    pre_.setValueAsPartOfGesture(juce::jlimit(0.0f, 500.0f, timeAt(e.position.x) * 1000.0f));
  } else if (drag_ == kAttack) {                              // the handle sits halfway, so the edge ends twice as far
    const float xEnd = 2.0f * e.position.x - s.xPre;
    const float pre = value(ei::id::preDelay) * 0.001f;
    attack_.setValueAsPartOfGesture(xEnd <= s.xPre + 0.5f ? 0.0f : juce::jlimit(0.0f, 2.0f, timeAt(xEnd) - pre));
  } else {
    // A free point: anywhere in level, and in time anywhere between its two neighbours.
    const int n = drag_ - kFirstPoint;
    const float earliest = n == 0 ? 0.0f : s.time[n - 1], latest = n == kPoints - 1 ? 60.0f : s.time[n + 1];
    time_[n]->setValueAsPartOfGesture(juce::jlimit(earliest, juce::jmax(earliest, latest), timeAt(e.position.x)));
    level_[n]->setValueAsPartOfGesture(yToLevel(e.position.y));
  }
}

void EnvelopeDisplay::mouseUp(const juce::MouseEvent&) {
  if (drag_ == kPre) pre_.endGesture();
  else if (drag_ == kAttack) attack_.endGesture();
  else if (drag_ >= kFirstPoint) { time_[drag_ - kFirstPoint]->endGesture(); level_[drag_ - kFirstPoint]->endGesture(); }
  drag_ = kNone;
}

float EnvelopeDisplay::defaultOf(juce::ParameterAttachment&, juce::RangedAudioParameter& p) { return p.convertFrom0to1(p.getDefaultValue()); }

// Double-click: the handle goes back to where it started.
void EnvelopeDisplay::mouseDoubleClick(const juce::MouseEvent& e) {
  const int h = handleAt(e.position);
  if (h == kAttack) attack_.setValueAsCompleteGesture(0.0f);
  else if (h >= kFirstPoint) {
    const int n = h - kFirstPoint;
    time_[n]->setValueAsCompleteGesture(defaultOf(*time_[n], *state_.getParameter(ei::id::tail(n + 1, "Time"))));
    level_[n]->setValueAsCompleteGesture(defaultOf(*level_[n], *state_.getParameter(ei::id::tail(n + 1, "Level"))));
  }
}

void EnvelopeDisplay::paint(juce::Graphics& g) {
  const auto bounds = getLocalBounds().toFloat();
  drawFrame(g, bounds, "TAIL ENVELOPE");
  const auto p = plot();
  const Shape s = shape();

  {
    juce::Graphics::ScopedSaveState clip(g);
    g.reduceClipRegion(p.toNearestInt());

    // The corridor that never converges: its vanishing point wanders with Size and the LFO.
    const float sizeNorm = state_.getParameter(ei::id::size)->getValue();
    for (int i = 0; i < 11; ++i) {
      const float t = float(i) / 10.0f, sc = std::pow(1.0f - t, 1.7f);
      const float cx = p.getX() + p.getWidth() * (0.5f + (0.10f + 0.16f * sizeNorm) * std::sin(t * 4.2f + drift_ + lfo_ * 1.5f));
      const float cy = p.getY() + p.getHeight() * (0.48f - 0.16f * t);
      const auto r = juce::Rectangle<float>(p.getWidth() * 0.98f * sc + 10.0f, p.getHeight() * 1.05f * sc + 6.0f).withCentre({cx, cy});
      juce::Path box; box.addRectangle(r);
      box.applyTransform(juce::AffineTransform::rotation(t * (0.24f + 0.2f * lfo_), cx, cy));
      g.setColour(col::dim.withAlpha(0.16f + 0.2f * t)); g.strokePath(box, juce::PathStrokeType(0.7f));
    }

    // Time and level grid.
    g.setColour(col::line);
    for (float t : {0.001f, 0.01f, 0.1f, 1.0f, 10.0f}) g.drawVerticalLine(juce::roundToInt(timeToX(t)), p.getY(), s.bottom);
    for (float db : {0.0f, -24.0f, -48.0f}) {
      const float dashes[] = {1.0f, 5.0f};
      g.drawDashedLine({p.getX(), levelToY(db), p.getRight(), levelToY(db)}, dashes, 2, 1.0f);
    }

    // Live history, oldest on the left.
    const float colW = p.getWidth() / float(kHistory);
    juce::Path wetArea, wetLine;
    wetArea.startNewSubPath(p.getX(), s.bottom);
    for (int i = 0; i < kHistory; ++i) {
      const int k = (head_ + i) % kHistory;
      const float x = p.getX() + float(i) * colW;
      const float yIn = levelToY(inDb_[size_t(k)]), yWet = levelToY(wetDb_[size_t(k)]);
      if (inDb_[size_t(k)] > kBottomDb + 1.0f) { g.setColour(col::mint.withAlpha(0.5f)); g.fillRect(x, yIn, juce::jmax(1.0f, colW - 1.4f), s.bottom - yIn); }
      wetArea.lineTo(x, yWet);
      if (i == 0) wetLine.startNewSubPath(x, yWet); else wetLine.lineTo(x, yWet);
    }
    wetArea.lineTo(p.getRight(), s.bottom); wetArea.closeSubPath();
    g.setGradientFill(juce::ColourGradient(col::green.withAlpha(0.42f), 0, p.getY(), col::green.withAlpha(0.0f), 0, s.bottom, false));
    g.fillPath(wetArea);
    g.setColour(col::green.withAlpha(0.55f)); g.strokePath(wetLine, juce::PathStrokeType(1.0f));

    // Gate threshold.
    if (value(ei::id::gateOn) > 0.5f) {
      const float thr = value(ei::id::gateThreshold), y = levelToY(thr);
      const float dashes[] = {6.0f, 4.0f};
      g.setColour(col::lime.withAlpha(0.8f)); g.drawDashedLine({p.getX(), y, p.getRight(), y}, dashes, 2, 1.0f);
      g.setFont(dataFont(10.0f));
      g.drawText("GATE " + juce::String(thr, 0) + " dB", juce::Rectangle<float>(p.getX() + 6, y - 14, 120, 12).toNearestInt(), juce::Justification::centredLeft);
    }

    // The envelope: silence, attack ramp up to full level, then the five free points.
    juce::Path env;
    env.startNewSubPath(p.getX(), s.bottom); env.lineTo(s.xPre, s.bottom); env.lineTo(s.xAttack, s.yZero);
    for (auto& pt : s.pt) env.lineTo(pt);
    glowStroke(g, env, col::green, 2.2f);
    if (s.neverDies) {                                       // the last point is above silence: the tail stays there
      const auto last = s.pt[kPoints - 1];
      const float dashes[] = {5.0f, 4.0f};
      g.setColour(col::green.withAlpha(0.7f)); g.drawDashedLine({last.x, last.y, p.getRight(), last.y}, dashes, 2, 1.6f);
    }
  }

  // Axis labels.
  g.setFont(dataFont(10.0f)); g.setColour(col::dim);
  const std::pair<float, const char*> ticks[] = {{0.001f, "1 ms"}, {0.01f, "10 ms"}, {0.1f, "100 ms"}, {1.0f, "1 s"}, {10.0f, "10 s"}};
  for (auto& t : ticks) g.drawText(t.second, juce::Rectangle<float>(timeToX(t.first) + 3, s.bottom + 3, 60, 12).toNearestInt(), juce::Justification::centredLeft);
  for (float db : {0.0f, -24.0f, -48.0f})
    g.drawText(juce::String(int(db)) + " dB", juce::Rectangle<float>(p.getRight() - 62, levelToY(db) - 13, 60, 12).toNearestInt(), juce::Justification::centredRight);
  if (s.neverDies) {
    g.setColour(col::green); g.setFont(dataFont(13.0f));
    g.drawText(juce::String::fromUTF8("∞"), juce::Rectangle<float>(p.getRight() - 22, s.pt[kPoints - 1].y - 18, 20, 14).toNearestInt(), juce::Justification::centredRight);
  }

  auto label = [&](juce::Point<float> c, float dx, float dy, juce::Justification just, const juce::String& name, const juce::String& val) {
    auto box = juce::Rectangle<float>(120, 13).withPosition(c.x + dx, c.y + dy);
    if (just == juce::Justification::centredRight) box.setX(c.x + dx - 120); else if (just == juce::Justification::centred) box.setX(c.x + dx - 60);
    box.setX(juce::jlimit(bounds.getX() + 4, bounds.getRight() - 124, box.getX()));
    box.setY(juce::jlimit(bounds.getY() + 26, bounds.getBottom() - 30, box.getY()));
    g.setColour(col::green); g.setFont(labelFont(10.5f, true)); g.drawText(name, box.toNearestInt(), just);
    if (val.isNotEmpty()) { g.setColour(col::mint); g.setFont(dataFont(11.0f)); g.drawText(val, box.translated(0, 13).toNearestInt(), just); }
  };
  auto diamond = [&](int h, float size) {
    const auto c = handlePos(h, s);
    const float r = (h == hover_ || h == drag_) ? size + 2.0f : size;
    juce::Path d; d.startNewSubPath(c.x, c.y - r); d.lineTo(c.x + r, c.y); d.lineTo(c.x, c.y + r); d.lineTo(c.x - r, c.y); d.closeSubPath();
    g.setColour(col::voidBlack); g.fillPath(d);
    glowStroke(g, d, col::mint, 1.6f);
    return c;
  };

  // PRE and ATT: how the reverb comes in.
  const float pre = value(ei::id::preDelay), att = value(ei::id::attack);
  {
    const auto c = handlePos(kAttack, s);
    const bool hot = hover_ == kAttack || drag_ == kAttack;
    const auto circle = juce::Rectangle<float>(hot ? 13.0f : 10.0f, hot ? 13.0f : 10.0f).withCentre(c);
    g.setColour(col::voidBlack); g.fillEllipse(circle);
    juce::Path e; e.addEllipse(circle);
    glowStroke(g, e, hot ? col::mint : col::green, 1.5f);
    label(c, -10, -6, juce::Justification::centredRight, "ATT", hot ? juce::String(att, 2) + " s" : juce::String());
  }
  label(diamond(kPre, 7.0f), -12, -30, juce::Justification::centredRight, "PRE", juce::String(pre, pre < 10 ? 1 : 0) + " ms");

  // The five free points. Each shows its number; the one in hand spells out its time and level.
  for (int n = 0; n < kPoints; ++n) {
    const int h = kFirstPoint + n;
    const bool hot = h == hover_ || h == drag_;
    const auto c = diamond(h, 6.0f);
    const bool below = c.y < p.getY() + 40.0f;                // near the top edge the label goes underneath
    juce::String val;
    if (hot) val = juce::String(s.time[n], s.time[n] < 10 ? 2 : 1) + " s  " + (s.level[n] > 0 ? "+" : "") + juce::String(s.level[n], 1) + " dB";
    label(c, 0, below ? 10.0f : (hot ? -36.0f : -23.0f), juce::Justification::centred, juce::String(n + 1), val);
  }

  // Legend, top right.
  auto legend = bounds.withHeight(26).withTrimmedRight(14);
  auto item = [&](const juce::String& text, juce::Colour c, bool lamp, bool lit) {
    g.setFont(labelFont(10.5f)); g.setColour(c);
    const int w = juce::roundToInt(juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), text)) + 4;
    g.drawText(text, legend.removeFromRight(float(w)).toNearestInt(), juce::Justification::centredRight);
    auto mark = legend.removeFromRight(16).withSizeKeepingCentre(8, 8);
    if (lamp) { if (lit) glowDot(g, mark.getCentre(), 4.0f, c); else { g.setColour(c.withAlpha(0.5f)); g.drawEllipse(mark, 1.0f); } }
    else { g.setColour(c.withAlpha(0.6f)); g.fillRect(mark); }
    legend.removeFromRight(12);
  };
  item("IN", col::mint, false, false);
  item("REVERB", col::green, false, false);
  if (value(ei::id::gateOn) > 0.5f) item("GATE", col::lime, true, gateGain_ > 0.5f);
}

}  // namespace ui
