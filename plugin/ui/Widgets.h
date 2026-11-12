// Small parameter-bound controls: ring knob, segmented chips, toggles.
#pragma once
#include "Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace ui {

// Ring knob: no body, just an arc that fills. Bipolar knobs fill from 12 o'clock.
class Knob : public juce::Component {
public:
  Knob(juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& label, bool bipolar = false)
      : param_(*state.getParameter(paramId)), label_(label), bipolar_(bipolar) {
    slider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider_.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider_.setLookAndFeel(&look_);
    slider_.getProperties().set("bipolar", bipolar);
    slider_.onValueChange = [this] { repaint(); };
    addAndMakeVisible(slider_);
    attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, paramId, slider_);
  }
  ~Knob() override { slider_.setLookAndFeel(nullptr); }

  // Centre and ring radius in parent coordinates.
  void place(float cx, float cy, float radius) {
    radius_ = radius;
    setBounds(juce::Rectangle<float>(cx - 40.0f, cy - radius - 20.0f, 80.0f, 2.0f * radius + 40.0f).toNearestInt());
  }
  void setActive(bool active) { if (active != isEnabled()) { setEnabled(active); setAlpha(active ? 1.0f : 0.32f); } }

  void resized() override {
    const float side = 2.0f * radius_ + 8.0f;
    slider_.setBounds(juce::Rectangle<float>(side, side).withCentre(getLocalBounds().toFloat().getCentre()).toNearestInt());
  }
  void paint(juce::Graphics& g) override {
    auto r = getLocalBounds();
    g.setColour(col::dim); g.setFont(labelFont(10.5f));
    g.drawText(label_, r.removeFromTop(14), juce::Justification::centred);
    juce::String text = param_.getCurrentValueAsText();
    if (param_.getLabel().isNotEmpty()) text << " " << param_.getLabel();
    g.setColour(col::mint); g.setFont(dataFont(11.0f));
    g.drawText(text, r.removeFromBottom(14), juce::Justification::centred);
  }

private:
  struct Look : juce::LookAndFeel_V4 {
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override {
      const auto b = juce::Rectangle<int>(x, y, w, h).toFloat();
      const auto c = b.getCentre();
      const float r = juce::jmin(b.getWidth(), b.getHeight()) * 0.5f - 4.0f;
      const float a = a0 + pos * (a1 - a0);
      juce::Path track; track.addCentredArc(c.x, c.y, r, r, 0, a0, a1, true);
      g.setColour(col::line); g.strokePath(track, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
      const float from = bool(s.getProperties()["bipolar"]) ? 0.5f * (a0 + a1) : a0;
      if (std::abs(a - from) > 0.02f) {
        juce::Path fill; fill.addCentredArc(c.x, c.y, r, r, 0, juce::jmin(from, a), juce::jmax(from, a), true);
        glowStroke(g, fill, col::green, 3.0f);
      }
      const auto dir = juce::Point<float>(std::sin(a), -std::cos(a));
      g.setColour(col::mint); g.drawLine({c + dir * (r - 7.0f), c + dir * (r + 1.0f)}, 1.6f);
      g.setColour(col::dim); g.fillEllipse(juce::Rectangle<float>(2.8f, 2.8f).withCentre(c));
    }
  };

  juce::RangedAudioParameter& param_;
  juce::String label_;
  bool bipolar_;
  float radius_ = 20.0f;
  Look look_;
  juce::Slider slider_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment_;
};

// Segmented selector for a choice or boolean parameter.
class Chips : public juce::Component {
public:
  Chips(juce::AudioProcessorValueTreeState& state, const juce::String& paramId, juce::StringArray items)
      : param_(*state.getParameter(paramId)), items_(std::move(items)),
        attachment_(param_, [this](float v) { index_ = juce::roundToInt(v); repaint(); }) {
    attachment_.sendInitialUpdate();
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
  }
  int index() const { return index_; }
  void setActive(bool active) { if (active != isEnabled()) { setEnabled(active); setAlpha(active ? 1.0f : 0.32f); } }

  void paint(juce::Graphics& g) override {
    const float w = float(getWidth()) / float(items_.size());
    for (int i = 0; i < items_.size(); ++i) {
      auto r = juce::Rectangle<float>(float(i) * w + 1.0f, 0.5f, w - 2.0f, float(getHeight()) - 1.0f);
      const bool on = i == index_;
      if (on) { g.setColour(col::green.withAlpha(0.16f)); g.fillRect(r); }
      g.setColour(on ? col::green : col::line); g.drawRect(r, 1.0f);
      g.setColour(on ? col::mint : col::dim); g.setFont(labelFont(10.0f, on, 0.1f));
      g.drawText(items_[i], r.toNearestInt(), juce::Justification::centred);
    }
  }
  void mouseDown(const juce::MouseEvent& e) override {
    const int i = juce::jlimit(0, items_.size() - 1, int(float(e.x) / float(getWidth()) * float(items_.size())));
    attachment_.setValueAsCompleteGesture(float(i));
  }

private:
  juce::RangedAudioParameter& param_;
  juce::StringArray items_;
  int index_ = 0;
  juce::ParameterAttachment attachment_;
};

// On/off control. Led: a lamp with ON/OFF beside it. Box: a framed word that lights up.
class Toggle : public juce::Component {
public:
  enum class Style { Led, Box };
  Toggle(juce::AudioProcessorValueTreeState& state, const juce::String& paramId, Style style, juce::String text = {})
      : param_(*state.getParameter(paramId)), style_(style), text_(std::move(text)),
        attachment_(param_, [this](float v) { on_ = v > 0.5f; repaint(); }) {
    attachment_.sendInitialUpdate();
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
  }
  bool isOn() const { return on_; }
  void setActive(bool active) { if (active != isEnabled()) { setEnabled(active); setAlpha(active ? 1.0f : 0.32f); } }

  void paint(juce::Graphics& g) override {
    auto r = getLocalBounds().toFloat();
    if (style_ == Style::Led) {
      const auto c = juce::Point<float>(r.getRight() - 8.0f, r.getCentreY());
      if (on_) glowDot(g, c, 5.0f, col::green);
      else { g.setColour(col::dim); g.drawEllipse(juce::Rectangle<float>(10, 10).withCentre(c), 1.2f); }
      g.setColour(on_ ? col::mint : col::dim); g.setFont(labelFont(10.0f));
      g.drawText(on_ ? "ON" : "OFF", r.withTrimmedRight(18).toNearestInt(), juce::Justification::centredRight);
    } else {
      r = r.reduced(0.5f);
      if (on_) { g.setColour(col::green.withAlpha(0.16f)); g.fillRect(r); }
      g.setColour(on_ ? col::green : col::dim); g.drawRect(r, 1.0f);
      g.setColour(on_ ? col::mint : col::dim);
      g.setFont(labelFont(juce::jmin(13.0f, r.getHeight() * 0.62f), true, getHeight() > 24 ? 0.3f : 0.1f));
      g.drawText(text_, r.toNearestInt(), juce::Justification::centred);
    }
  }
  void mouseDown(const juce::MouseEvent&) override { attachment_.setValueAsCompleteGesture(on_ ? 0.0f : 1.0f); }

private:
  juce::RangedAudioParameter& param_;
  Style style_;
  juce::String text_;
  bool on_ = false;
  juce::ParameterAttachment attachment_;
};

}  // namespace ui
