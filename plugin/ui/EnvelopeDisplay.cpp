#include "EnvelopeDisplay.h"
#include "Parameters.h"

namespace ui {
namespace {
constexpr float kNeutralPower = 2.6f;   // how the plain exponential release is drawn
}

EnvelopeDisplay::EnvelopeDisplay(juce::AudioProcessorValueTreeState& state)
    : state_(state),
      pre_(*state.getParameter(ei::id::preDelay), [this](float) { repaint(); }),
      attack_(*state.getParameter(ei::id::attack), [this](float) { repaint(); }),
      sus_(*state.getParameter(ei::id::sustain), [this](float) { repaint(); }),
      bender_(*state.getParameter(ei::id::bender), [this](float) { repaint(); }),
      rel_(*state.getParameter(ei::id::release), [this](float) { repaint(); }),
      curve_(*state.getParameter(ei::id::curve), [this](float) { repaint(); }),
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

// Envelope level scale: 0 dB on `top`, +12 dB near the top edge, -24 dB 60 % of the way down.
float EnvelopeDisplay::bendToY(float db, const Shape& s) const {
  const float up = s.top - (plot().getY() + 8.0f), down = (s.bottom - s.top) * 0.6f;
  return db >= 0.0f ? s.top - db / 12.0f * up : s.top + (-db / 24.0f) * down;
}
float EnvelopeDisplay::yToBend(float y, const Shape& s) const {
  const float up = s.top - (plot().getY() + 8.0f), down = (s.bottom - s.top) * 0.6f;
  return juce::jlimit(-24.0f, 12.0f, y <= s.top ? (s.top - y) / up * 12.0f : -(y - s.top) / down * 24.0f);
}

EnvelopeDisplay::Shape EnvelopeDisplay::shape() const {
  const auto p = plot();
  const float pre = value(ei::id::preDelay) * 0.001f, att = value(ei::id::attack), sus = value(ei::id::sustain), rel = value(ei::id::release);
  Shape s;
  s.bottom = p.getBottom(); s.top = p.getY() + p.getHeight() * 0.3f;
  s.infinite = value(ei::id::releaseInf) > 0.5f; s.hasSustain = sus > 0.01f;
  s.xPre = timeToX(pre); s.xAttack = timeToX(pre + att); s.xSus = timeToX(pre + att + sus);
  s.xRel = s.infinite ? p.getRight() : timeToX(pre + att + sus + rel);
  s.ySusEnd = s.hasSustain ? bendToY(value(ei::id::bender), s) : s.top;
  s.power = kNeutralPower * std::pow(4.0f, -value(ei::id::curve) * 0.01f);
  return s;
}

bool EnvelopeDisplay::handleVisible(Handle h, const Shape& s) const {
  if (h == Bender) return s.hasSustain;
  if (h == Curve) return !s.infinite;
  return true;
}

juce::Point<float> EnvelopeDisplay::handlePos(Handle h, const Shape& s) const {
  switch (h) {
    case Pre:    return {s.xPre, s.bottom};
    case Attack: return {0.5f * (s.xPre + s.xAttack), 0.5f * (s.bottom + s.top)};        // halfway up the rising edge
    case Sus:    return {s.xSus, s.ySusEnd};
    case Bender: return {0.5f * (s.xAttack + s.xSus), 0.5f * (s.top + s.ySusEnd)};       // halfway along the sustain
    case Rel:    return s.infinite ? juce::Point<float>(s.xRel, s.ySusEnd) : juce::Point<float>(s.xRel, s.bottom);
    case Curve:  return {0.5f * (s.xSus + s.xRel), s.ySusEnd + (s.bottom - s.ySusEnd) * (1.0f - std::pow(0.5f, s.power))};
    default:     return {};
  }
}

EnvelopeDisplay::Handle EnvelopeDisplay::handleAt(juce::Point<float> pt) const {
  const Shape s = shape();
  Handle best = None; float bestDist = 14.0f;
  for (int i = 0; i < NumHandles; ++i) {
    const Handle h = Handle(i);
    if (!handleVisible(h, s)) continue;
    const float d = handlePos(h, s).getDistanceFrom(pt);
    if (d < bestDist) { bestDist = d; best = h; }
  }
  return best;
}

juce::ParameterAttachment* EnvelopeDisplay::attachmentFor(Handle h) {
  switch (h) {
    case Pre: return &pre_;  case Attack: return &attack_;  case Sus: return &sus_;
    case Bender: return &bender_;  case Rel: return &rel_;  case Curve: return &curve_;
    default: return nullptr;
  }
}

void EnvelopeDisplay::mouseMove(const juce::MouseEvent& e) {
  const Handle h = handleAt(e.position);
  if (h != hover_) { hover_ = h; repaint(); }
  setMouseCursor(h == None ? juce::MouseCursor::NormalCursor
                           : (isVertical(h) ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::LeftRightResizeCursor));
}
void EnvelopeDisplay::mouseDown(const juce::MouseEvent& e) {
  drag_ = handleAt(e.position);
  if (auto* a = attachmentFor(drag_)) a->beginGesture();
}
void EnvelopeDisplay::mouseDrag(const juce::MouseEvent& e) {
  if (drag_ == None) return;
  const auto p = plot();
  const Shape s = shape();
  const float pre = value(ei::id::preDelay) * 0.001f, att = value(ei::id::attack), sus = value(ei::id::sustain);
  auto timeAt = [&](float x) { return x <= p.getX() + 1.0f ? 0.0f : xToTime(x); };
  switch (drag_) {
    case Pre:    pre_.setValueAsPartOfGesture(juce::jlimit(0.0f, 500.0f, timeAt(e.position.x) * 1000.0f)); break;
    case Attack: {                                              // the handle sits halfway, so the edge ends twice as far
      const float xEnd = 2.0f * e.position.x - s.xPre;
      attack_.setValueAsPartOfGesture(xEnd <= s.xPre + 0.5f ? 0.0f : juce::jlimit(0.0f, 2.0f, timeAt(xEnd) - pre));
      break;
    }
    case Sus:    sus_.setValueAsPartOfGesture(juce::jlimit(0.0f, 10.0f, timeAt(e.position.x) - pre - att)); break;
    case Bender: bender_.setValueAsPartOfGesture(yToBend(2.0f * e.position.y - s.top, s)); break;
    case Rel:    if (!s.infinite) rel_.setValueAsPartOfGesture(juce::jlimit(0.1f, 60.0f, timeAt(e.position.x) - pre - att - sus)); break;
    case Curve: {
      const float span = juce::jmax(1.0f, s.bottom - s.ySusEnd);
      const float f = juce::jlimit(0.03f, 0.97f, (e.position.y - s.ySusEnd) / span);     // how far the midpoint has fallen
      const float power = std::log(1.0f - f) / std::log(0.5f);
      curve_.setValueAsPartOfGesture(juce::jlimit(-100.0f, 100.0f, -100.0f * std::log(power / kNeutralPower) / std::log(4.0f)));
      break;
    }
    default: break;
  }
}
void EnvelopeDisplay::mouseUp(const juce::MouseEvent&) {
  if (auto* a = attachmentFor(drag_)) a->endGesture();
  drag_ = None;
}
void EnvelopeDisplay::mouseDoubleClick(const juce::MouseEvent& e) {
  switch (handleAt(e.position)) {
    case Rel:    inf_.setValueAsCompleteGesture(value(ei::id::releaseInf) > 0.5f ? 0.0f : 1.0f); break;
    case Attack: attack_.setValueAsCompleteGesture(0.0f); break;      // the three shaping handles snap back to neutral
    case Bender: bender_.setValueAsCompleteGesture(0.0f); break;
    case Curve:  curve_.setValueAsCompleteGesture(0.0f); break;
    default: break;
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
    for (float db : {-12.0f, -36.0f, -60.0f}) {
      const float dashes[] = {1.0f, 5.0f};
      g.drawDashedLine({p.getX(), dbToY(db), p.getRight(), dbToY(db)}, dashes, 2, 1.0f);
    }

    // Live history, oldest on the left.
    const float colW = p.getWidth() / float(kHistory);
    juce::Path wetArea, wetLine;
    wetArea.startNewSubPath(p.getX(), s.bottom);
    for (int i = 0; i < kHistory; ++i) {
      const int k = (head_ + i) % kHistory;
      const float x = p.getX() + float(i) * colW;
      const float yIn = dbToY(inDb_[size_t(k)]), yWet = dbToY(wetDb_[size_t(k)]);
      if (inDb_[size_t(k)] > kFloorDb + 1.0f) { g.setColour(col::mint.withAlpha(0.5f)); g.fillRect(x, yIn, juce::jmax(1.0f, colW - 1.4f), s.bottom - yIn); }
      wetArea.lineTo(x, yWet);
      if (i == 0) wetLine.startNewSubPath(x, yWet); else wetLine.lineTo(x, yWet);
    }
    wetArea.lineTo(p.getRight(), s.bottom); wetArea.closeSubPath();
    g.setGradientFill(juce::ColourGradient(col::green.withAlpha(0.42f), 0, p.getY(), col::green.withAlpha(0.0f), 0, s.bottom, false));
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

    // The envelope: silence, attack ramp, sustain (possibly bent), release (possibly curved).
    juce::Path env;
    env.startNewSubPath(p.getX(), s.bottom); env.lineTo(s.xPre, s.bottom); env.lineTo(s.xAttack, s.top); env.lineTo(s.xSus, s.ySusEnd);
    if (s.infinite) env.lineTo(p.getRight(), s.ySusEnd);
    else for (int i = 1; i <= 48; ++i) {
      const float u = float(i) / 48.0f;
      env.lineTo(s.xSus + (s.xRel - s.xSus) * u, s.ySusEnd + (s.bottom - s.ySusEnd) * (1.0f - std::pow(1.0f - u, s.power)));
    }
    glowStroke(g, env, col::green, 2.2f);
  }

  // Axis labels.
  g.setFont(dataFont(10.0f)); g.setColour(col::dim);
  const std::pair<float, const char*> ticks[] = {{0.001f, "1 ms"}, {0.01f, "10 ms"}, {0.1f, "100 ms"}, {1.0f, "1 s"}, {10.0f, "10 s"}};
  for (auto& t : ticks) g.drawText(t.second, juce::Rectangle<float>(timeToX(t.first) + 3, s.bottom + 3, 60, 12).toNearestInt(), juce::Justification::centredLeft);
  for (float db : {-12.0f, -36.0f, -60.0f})
    g.drawText(juce::String(int(db)) + " dB", juce::Rectangle<float>(p.getRight() - 62, dbToY(db) - 13, 60, 12).toNearestInt(), juce::Justification::centredRight);

  // Handles. The three timing ones are diamonds with their value always shown; the three shaping ones are
  // smaller rings that only spell out their value while pointed at or dragged.
  auto label = [&](juce::Point<float> c, float dx, float dy, juce::Justification just, const juce::String& name, const juce::String& val) {
    auto box = juce::Rectangle<float>(84, 13).withPosition(c.x + dx, c.y + dy);
    if (just == juce::Justification::centredRight) box.setX(c.x + dx - 84); else if (just == juce::Justification::centred) box.setX(c.x + dx - 42);
    box.setX(juce::jlimit(bounds.getX() + 4, bounds.getRight() - 88, box.getX()));
    box.setY(juce::jlimit(bounds.getY() + 26, bounds.getBottom() - 28, box.getY()));
    g.setColour(col::green); g.setFont(labelFont(10.5f, true)); g.drawText(name, box.toNearestInt(), just);
    if (val.isNotEmpty()) { g.setColour(col::mint); g.setFont(dataFont(11.5f)); g.drawText(val, box.translated(0, 13).toNearestInt(), just); }
  };
  auto diamond = [&](Handle h, float dx, float dy, juce::Justification just, const juce::String& name, const juce::String& val) {
    const auto c = handlePos(h, s);
    const float r = (h == hover_ || h == drag_) ? 9.0f : 7.0f;
    juce::Path d; d.startNewSubPath(c.x, c.y - r); d.lineTo(c.x + r, c.y); d.lineTo(c.x, c.y + r); d.lineTo(c.x - r, c.y); d.closeSubPath();
    g.setColour(col::voidBlack); g.fillPath(d);
    glowStroke(g, d, col::mint, 1.6f);
    label(c, dx, dy, just, name, val);
  };
  auto ring = [&](Handle h, float dx, float dy, juce::Justification just, const juce::String& name, const juce::String& val) {
    if (!handleVisible(h, s)) return;
    const auto c = handlePos(h, s);
    const bool hot = h == hover_ || h == drag_;
    const auto circle = juce::Rectangle<float>(hot ? 13.0f : 10.0f, hot ? 13.0f : 10.0f).withCentre(c);
    g.setColour(col::voidBlack); g.fillEllipse(circle);
    juce::Path e; e.addEllipse(circle);
    glowStroke(g, e, hot ? col::mint : col::green, 1.5f);
    label(c, dx, dy, just, name, hot ? val : juce::String());
  };
  const float pre = value(ei::id::preDelay), att = value(ei::id::attack), sus = value(ei::id::sustain), rel = value(ei::id::release);
  const float bend = value(ei::id::bender), curve = value(ei::id::curve);
  auto signedText = [](float v, int decimals, const char* unit) { return (v > 0 ? "+" : "") + juce::String(v, decimals) + unit; };

  ring(Attack, 10, -6, juce::Justification::centredLeft, "ATT", juce::String(att, 2) + " s");
  ring(Bender, 0, bend >= 0 ? 10.0f : -36.0f, juce::Justification::centred, "BEND", signedText(bend, 1, " dB"));
  ring(Curve, 12, -8, juce::Justification::centredLeft, "CURVE", signedText(curve, 0, " %"));
  diamond(Pre, -12, -30, juce::Justification::centredRight, "PRE", juce::String(pre, pre < 10 ? 1 : 0) + " ms");
  diamond(Sus, 0, -38, juce::Justification::centred, "SUS", juce::String(sus, 2) + " s");
  if (s.infinite) diamond(Rel, -12, 12, juce::Justification::centredRight, "REL", juce::String::fromUTF8("∞"));
  else diamond(Rel, 12, -30, juce::Justification::centredLeft, "REL", juce::String(rel, rel < 10 ? 2 : 1) + " s");

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
