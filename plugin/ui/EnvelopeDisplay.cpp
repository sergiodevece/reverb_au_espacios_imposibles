#include "EnvelopeDisplay.h"
#include "Parameters.h"

namespace ui {

EnvelopeDisplay::EnvelopeDisplay(juce::AudioProcessorValueTreeState& state)
    : state_(state),
      pre_(*state.getParameter(ei::id::preDelay), [this](float) { repaint(); }),
      sus_(*state.getParameter(ei::id::sustain), [this](float) { repaint(); }),
      rel_(*state.getParameter(ei::id::release), [this](float) { repaint(); }),
      inf_(*state.getParameter(ei::id::releaseInf), [this](float) { repaint(); }) {
  inDb_.fill(kFloorDb); wetDb_.fill(kFloorDb);
}

float EnvelopeDisplay::value(const char* paramId) const { return state_.getRawParameterValue(paramId)->load(); }

void EnvelopeDisplay::pushFrame(float inputPeak, float wetPeak, float gateGain, float lfoValue) {
  inDb_[size_t(head_)] = juce::jmax(kFloorDb, juce::Decibels::gainToDecibels(inputPeak, kFloorDb));
  wetDb_[size_t(head_)] = juce::jmax(kFloorDb, juce::Decibels::gainToDecibels(wetPeak, kFloorDb));
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
float EnvelopeDisplay::dbToY(float db) const {
  const auto p = plot();
  return p.getY() + p.getHeight() * juce::jlimit(0.0f, 1.0f, db / kFloorDb);
}

juce::Point<float> EnvelopeDisplay::handlePos(Handle h) const {
  const auto p = plot();
  const float top = p.getY() + p.getHeight() * 0.1f;
  const float pre = value(ei::id::preDelay) * 0.001f, sus = value(ei::id::sustain), rel = value(ei::id::release);
  switch (h) {
    case Pre: return {timeToX(pre), p.getBottom()};
    case Sus: return {timeToX(pre + sus), top};
    case Rel: return value(ei::id::releaseInf) > 0.5f ? juce::Point<float>(p.getRight(), top)
                                                       : juce::Point<float>(timeToX(pre + sus + rel), p.getBottom());
    default: return {};
  }
}

EnvelopeDisplay::Handle EnvelopeDisplay::handleAt(juce::Point<float> pt) const {
  Handle best = None; float bestDist = 16.0f;
  for (Handle h : {Rel, Sus, Pre}) {
    const float d = handlePos(h).getDistanceFrom(pt);
    if (d < bestDist) { bestDist = d; best = h; }
  }
  return best;
}

void EnvelopeDisplay::mouseMove(const juce::MouseEvent& e) {
  const Handle h = handleAt(e.position);
  if (h != hover_) { hover_ = h; repaint(); }
  setMouseCursor(h == None ? juce::MouseCursor::NormalCursor : juce::MouseCursor::LeftRightResizeCursor);
}
void EnvelopeDisplay::mouseDown(const juce::MouseEvent& e) {
  drag_ = handleAt(e.position);
  if (drag_ == Pre) pre_.beginGesture();
  if (drag_ == Sus) sus_.beginGesture();
  if (drag_ == Rel) rel_.beginGesture();
}
void EnvelopeDisplay::mouseDrag(const juce::MouseEvent& e) {
  if (drag_ == None) return;
  const auto p = plot();
  const float pre = value(ei::id::preDelay) * 0.001f, sus = value(ei::id::sustain);
  const float t = e.position.x <= p.getX() + 1.0f ? 0.0f : xToTime(e.position.x);
  if (drag_ == Pre) pre_.setValueAsPartOfGesture(juce::jlimit(0.0f, 500.0f, t * 1000.0f));
  if (drag_ == Sus) sus_.setValueAsPartOfGesture(juce::jlimit(0.0f, 10.0f, t - pre));
  if (drag_ == Rel && value(ei::id::releaseInf) < 0.5f) rel_.setValueAsPartOfGesture(juce::jlimit(0.1f, 60.0f, t - pre - sus));
}
void EnvelopeDisplay::mouseUp(const juce::MouseEvent&) {
  if (drag_ == Pre) pre_.endGesture();
  if (drag_ == Sus) sus_.endGesture();
  if (drag_ == Rel) rel_.endGesture();
  drag_ = None;
}
void EnvelopeDisplay::mouseDoubleClick(const juce::MouseEvent& e) {
  if (handleAt(e.position) == Rel) inf_.setValueAsCompleteGesture(value(ei::id::releaseInf) > 0.5f ? 0.0f : 1.0f);
}

void EnvelopeDisplay::paint(juce::Graphics& g) {
  const auto bounds = getLocalBounds().toFloat();
  drawFrame(g, bounds, "TAIL ENVELOPE");
  const auto p = plot();
  const float bottom = p.getBottom(), top = p.getY() + p.getHeight() * 0.1f;

  {
    juce::Graphics::ScopedSaveState clip(g);
    g.reduceClipRegion(p.toNearestInt());

    // The corridor that never converges: its vanishing point wanders with Size and the LFO.
    const float sizeNorm = state_.getParameter(ei::id::size)->getValue();
    for (int i = 0; i < 11; ++i) {
      const float t = float(i) / 10.0f, s = std::pow(1.0f - t, 1.7f);
      const float cx = p.getX() + p.getWidth() * (0.5f + (0.10f + 0.16f * sizeNorm) * std::sin(t * 4.2f + drift_ + lfo_ * 1.5f));
      const float cy = p.getY() + p.getHeight() * (0.48f - 0.16f * t);
      const auto r = juce::Rectangle<float>(p.getWidth() * 0.98f * s + 10.0f, p.getHeight() * 1.05f * s + 6.0f).withCentre({cx, cy});
      juce::Path box; box.addRectangle(r);
      box.applyTransform(juce::AffineTransform::rotation(t * (0.24f + 0.2f * lfo_), cx, cy));
      g.setColour(col::dim.withAlpha(0.16f + 0.2f * t)); g.strokePath(box, juce::PathStrokeType(0.7f));
    }

    // Time and level grid.
    g.setColour(col::line);
    for (float t : {0.001f, 0.01f, 0.1f, 1.0f, 10.0f}) g.drawVerticalLine(juce::roundToInt(timeToX(t)), p.getY(), bottom);
    for (float db : {-12.0f, -36.0f, -60.0f}) {
      const float dashes[] = {1.0f, 5.0f};
      g.drawDashedLine({p.getX(), dbToY(db), p.getRight(), dbToY(db)}, dashes, 2, 1.0f);
    }

    // Live history, oldest on the left.
    const float colW = p.getWidth() / float(kHistory);
    juce::Path wetArea, wetLine;
    wetArea.startNewSubPath(p.getX(), bottom);
    for (int i = 0; i < kHistory; ++i) {
      const int k = (head_ + i) % kHistory;
      const float x = p.getX() + float(i) * colW;
      const float yIn = dbToY(inDb_[size_t(k)]), yWet = dbToY(wetDb_[size_t(k)]);
      if (inDb_[size_t(k)] > kFloorDb + 1.0f) { g.setColour(col::mint.withAlpha(0.5f)); g.fillRect(x, yIn, juce::jmax(1.0f, colW - 1.4f), bottom - yIn); }
      wetArea.lineTo(x, yWet);
      if (i == 0) wetLine.startNewSubPath(x, yWet); else wetLine.lineTo(x, yWet);
    }
    wetArea.lineTo(p.getRight(), bottom); wetArea.closeSubPath();
    g.setGradientFill(juce::ColourGradient(col::green.withAlpha(0.42f), 0, p.getY(), col::green.withAlpha(0.0f), 0, bottom, false));
    g.fillPath(wetArea);
    g.setColour(col::green.withAlpha(0.55f)); g.strokePath(wetLine, juce::PathStrokeType(1.0f));

    // Gate threshold.
    if (value(ei::id::gateOn) > 0.5f) {
      const float thr = value(ei::id::gateThreshold), y = dbToY(thr);
      const float dashes[] = {6.0f, 4.0f};
      g.setColour(col::lime.withAlpha(0.8f)); g.drawDashedLine({p.getX(), y, p.getRight(), y}, dashes, 2, 1.0f);
      g.setFont(dataFont(10.0f));
      g.drawText("GATE " + juce::String(thr, 0) + " dB", juce::Rectangle<float>(p.getX() + 6, y - 14, 120, 12).toNearestInt(), juce::Justification::centredLeft);
    }

    // The envelope itself.
    const bool infinite = value(ei::id::releaseInf) > 0.5f;
    const auto a = handlePos(Pre), b = handlePos(Sus), c = handlePos(Rel);
    juce::Path env;
    env.startNewSubPath(p.getX(), bottom); env.lineTo(a.x, bottom); env.lineTo(a.x, top); env.lineTo(b.x, top);
    if (infinite) env.lineTo(p.getRight(), top);
    else for (int i = 1; i <= 40; ++i) { const float u = float(i) / 40.0f; env.lineTo(b.x + (c.x - b.x) * u, top + (bottom - top) * (1.0f - std::pow(1.0f - u, 2.6f))); }
    glowStroke(g, env, col::green, 2.2f);
  }

  // Axis labels.
  g.setFont(dataFont(10.0f)); g.setColour(col::dim);
  const std::pair<float, const char*> ticks[] = {{0.001f, "1 ms"}, {0.01f, "10 ms"}, {0.1f, "100 ms"}, {1.0f, "1 s"}, {10.0f, "10 s"}};
  for (auto& t : ticks) g.drawText(t.second, juce::Rectangle<float>(timeToX(t.first) + 3, bottom + 3, 60, 12).toNearestInt(), juce::Justification::centredLeft);
  for (float db : {-12.0f, -36.0f, -60.0f})
    g.drawText(juce::String(int(db)) + " dB", juce::Rectangle<float>(p.getRight() - 62, dbToY(db) - 13, 60, 12).toNearestInt(), juce::Justification::centredRight);

  // Handles with their read-outs.
  auto drawHandle = [&](Handle h, const juce::String& name, const juce::String& val, float dx, float dy, juce::Justification just) {
    const auto c = handlePos(h);
    const float s = (h == hover_ || h == drag_) ? 9.0f : 7.0f;
    juce::Path d; d.startNewSubPath(c.x, c.y - s); d.lineTo(c.x + s, c.y); d.lineTo(c.x, c.y + s); d.lineTo(c.x - s, c.y); d.closeSubPath();
    g.setColour(col::voidBlack); g.fillPath(d);
    glowStroke(g, d, col::mint, 1.6f);
    auto box = juce::Rectangle<float>(80, 13).withPosition(c.x + dx, c.y + dy);
    if (just == juce::Justification::centredRight) box.setX(c.x + dx - 80); else if (just == juce::Justification::centred) box.setX(c.x + dx - 40);
    box.setX(juce::jlimit(bounds.getX() + 4, bounds.getRight() - 84, box.getX()));
    g.setColour(col::green); g.setFont(labelFont(10.5f, true)); g.drawText(name, box.toNearestInt(), just);
    g.setColour(col::mint); g.setFont(dataFont(11.5f)); g.drawText(val, box.translated(0, 13).toNearestInt(), just);
  };
  const float pre = value(ei::id::preDelay), sus = value(ei::id::sustain), rel = value(ei::id::release);
  drawHandle(Pre, "PRE", juce::String(pre, pre < 10 ? 1 : 0) + " ms", 12, -30, juce::Justification::centredLeft);
  drawHandle(Sus, "SUS", juce::String(sus, 2) + " s", 0, -38, juce::Justification::centred);
  if (value(ei::id::releaseInf) > 0.5f) drawHandle(Rel, "REL", juce::String::fromUTF8("∞"), -12, 12, juce::Justification::centredRight);
  else drawHandle(Rel, "REL", juce::String(rel, rel < 10 ? 2 : 1) + " s", 12, -30, juce::Justification::centredLeft);

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
