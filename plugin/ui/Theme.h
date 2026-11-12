// Colours, fonts and shared drawing helpers: black ground, phosphor-green line work.
#pragma once
#include <BinaryData.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace ui {

namespace col {
const juce::Colour voidBlack{0xff000000};
const juce::Colour panel{0xff050a07};
const juce::Colour line{0xff12331f};
const juce::Colour dim{0xff1f7a4a};
const juce::Colour green{0xff2bff88};
const juce::Colour mint{0xffc9ffe0};
const juce::Colour lime{0xffb6ff3c};
}  // namespace col

struct Fonts {
  juce::Typeface::Ptr display = juce::Typeface::createSystemTypefaceFor(BinaryData::MichromaRegular_ttf, BinaryData::MichromaRegular_ttfSize);
  juce::Typeface::Ptr label = juce::Typeface::createSystemTypefaceFor(BinaryData::ChakraPetchMedium_ttf, BinaryData::ChakraPetchMedium_ttfSize);
  juce::Typeface::Ptr labelBold = juce::Typeface::createSystemTypefaceFor(BinaryData::ChakraPetchSemiBold_ttf, BinaryData::ChakraPetchSemiBold_ttfSize);
  juce::Typeface::Ptr data = juce::Typeface::createSystemTypefaceFor(BinaryData::JetBrainsMonoRegular_ttf, BinaryData::JetBrainsMonoRegular_ttfSize);
};
inline const Fonts& fonts() { static Fonts f; return f; }

inline juce::Font makeFont(const juce::Typeface::Ptr& tf, float height, float kerning) {
  return juce::Font(juce::FontOptions().withTypeface(tf).withHeight(height)).withExtraKerningFactor(kerning);
}
inline juce::Font displayFont(float h, float kerning = 0.2f) { return makeFont(fonts().display, h, kerning); }
inline juce::Font labelFont(float h = 11.0f, bool bold = false, float kerning = 0.12f) { return makeFont(bold ? fonts().labelBold : fonts().label, h, kerning); }
inline juce::Font dataFont(float h = 11.5f) { return makeFont(fonts().data, h, 0.0f); }

// A bright stroke with a soft halo underneath.
inline void glowStroke(juce::Graphics& g, const juce::Path& p, juce::Colour c, float width) {
  g.setColour(c.withAlpha(0.10f)); g.strokePath(p, juce::PathStrokeType(width + 6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
  g.setColour(c.withAlpha(0.22f)); g.strokePath(p, juce::PathStrokeType(width + 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
  g.setColour(c); g.strokePath(p, juce::PathStrokeType(width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
inline void glowDot(juce::Graphics& g, juce::Point<float> c, float r, juce::Colour colour) {
  g.setColour(colour.withAlpha(0.18f)); g.fillEllipse(juce::Rectangle<float>(r * 4, r * 4).withCentre(c));
  g.setColour(colour); g.fillEllipse(juce::Rectangle<float>(r * 2, r * 2).withCentre(c));
}

// Section frame: hairline box with brighter corner ticks and a title.
inline void drawFrame(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title) {
  g.setColour(col::panel); g.fillRect(r);
  g.setColour(col::line); g.drawRect(r, 1.0f);
  const float c = 8.0f;
  juce::Path ticks;
  auto corner = [&](float x, float y, float sx, float sy) { ticks.startNewSubPath(x + sx * c, y); ticks.lineTo(x, y); ticks.lineTo(x, y + sy * c); };
  corner(r.getX() + 0.5f, r.getY() + 0.5f, 1, 1); corner(r.getRight() - 0.5f, r.getY() + 0.5f, -1, 1);
  corner(r.getX() + 0.5f, r.getBottom() - 0.5f, 1, -1); corner(r.getRight() - 0.5f, r.getBottom() - 0.5f, -1, -1);
  g.setColour(col::dim); g.strokePath(ticks, juce::PathStrokeType(1.2f));
  if (title.isNotEmpty()) {
    g.setColour(col::green); g.setFont(labelFont(12.0f, true, 0.2f));
    g.drawText(title, r.withTrimmedLeft(12).withHeight(26).toNearestInt(), juce::Justification::centredLeft);
  }
}

}  // namespace ui
