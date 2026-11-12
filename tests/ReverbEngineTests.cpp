#include "TestHarness.h"
#include "dsp/ReverbEngine.h"
#include <random>

namespace {
struct Rig {
  ei::ReverbEngine e; ei::EngineParams p; double sr;
  explicit Rig(double sampleRate = 48000) : sr(sampleRate) { e.prepare(sr, 512); p.lfoDepth = 0; p.mix = 1; e.setParams(p); }
  // Feeds `in` (mono, to both channels) in blocks; returns the left output.
  std::vector<float> run(const std::vector<float>& in, int block = 512, std::vector<float>* right = nullptr) {
    std::vector<float> l(in), r(in);
    for (size_t o = 0; o < in.size(); o += size_t(block)) {
      int n = int(std::min(size_t(block), in.size() - o));
      e.setParams(p); e.process(l.data() + o, r.data() + o, n);
    }
    if (right) *right = r;
    return l;
  }
};
std::vector<float> noiseBurst(double sr, double burstSeconds, double totalSeconds, unsigned seed = 7, float amp = 0.5f) {
  std::mt19937 rng(seed); std::uniform_real_distribution<float> u(-amp, amp);
  std::vector<float> v(size_t(totalSeconds * sr), 0.0f);
  for (size_t i = 0; i < size_t(burstSeconds * sr) && i < v.size(); ++i) v[i] = u(rng);
  return v;
}
float levelAt(const std::vector<float>& y, double sr, double t, double window = 0.05) { return th::rmsDb(y, int(t * sr), int(window * sr)); }
bool allFinite(const std::vector<float>& y, float& peak) {
  bool ok = true; peak = 0;
  for (float v : y) { ok = ok && std::isfinite(v); peak = std::fmax(peak, std::fabs(v)); }
  return ok;
}
}

TEST(Engine_dryPassthroughAtMixZero) {
  Rig r; r.p.mix = 0; r.e.setParams(r.p); r.e.reset();
  auto in = noiseBurst(r.sr, 1.0, 1.0);
  auto out = r.run(in);
  double maxErr = 0; for (size_t i = 0; i < in.size(); ++i) maxErr = std::fmax(maxErr, std::fabs(out[i] - in[i]));
  CHECK(maxErr < 1e-6);
}

TEST(Engine_preDelayOffsetsOnset) {
  Rig r; r.p.preDelayMs = 100; r.e.setParams(r.p); r.e.reset();
  std::vector<float> in(size_t(r.sr), 0.0f); in[0] = 1.0f;
  auto out = r.run(in);
  float early = 0; for (int i = 0; i < int(0.099 * r.sr); ++i) early = std::fmax(early, std::fabs(out[i]));
  CHECK(early < 1e-4f);
  CHECK(levelAt(out, r.sr, 0.12, 0.2) > -70.0f);
}

TEST(Engine_sustainThenRelease) {
  Rig r; r.p.sustainSec = 2; r.p.releaseSec = 1; r.e.setParams(r.p); r.e.reset();
  auto out = r.run(noiseBurst(r.sr, 0.1, 4.5));
  float a = levelAt(out, r.sr, 0.5, 0.2), b = levelAt(out, r.sr, 1.5, 0.2), c = levelAt(out, r.sr, 4.0, 0.2);
  CHECK(a > -60.0f);
  CHECK_NEAR(a, b, 3.0);
  CHECK(c < a - 40.0f);
}

TEST(Engine_gatedReverbCuts) {
  auto in = noiseBurst(48000, 0.1, 1.0);
  Rig open; open.p.releaseSec = 4; open.e.setParams(open.p); open.e.reset();
  auto ungated = open.run(in);
  Rig g; g.p.releaseSec = 4; g.p.gateOn = true; g.p.gateSource = ei::EngineParams::GateSource::Dry;
  g.p.gateThresholdDb = -30; g.p.gateHoldMs = 100; g.p.gateReleaseMs = 50; g.p.gateRangeDb = -80; g.e.setParams(g.p); g.e.reset();
  auto gated = g.run(in);
  float peak = -200; for (double t = 0.0; t < 0.3; t += 0.02) peak = std::fmax(peak, levelAt(ungated, 48000, t, 0.02));
  CHECK(levelAt(ungated, 48000, 0.5, 0.02) > peak - 20.0f);
  CHECK(levelAt(gated, 48000, 0.5, 0.02) < peak - 60.0f);
  CHECK(levelAt(gated, 48000, 0.08, 0.02) > peak - 12.0f);     // and it is open while the source plays
}

TEST(Engine_gateFromWetFollowsTail) {
  Rig g; g.p.releaseSec = 1; g.p.gateOn = true; g.p.gateSource = ei::EngineParams::GateSource::Wet;
  g.p.gateThresholdDb = -20; g.p.gateHoldMs = 0; g.p.gateReleaseMs = 20; g.e.setParams(g.p); g.e.reset();
  auto out = g.run(noiseBurst(48000, 0.1, 3.0));
  CHECK(levelAt(out, 48000, 0.1) > -40.0f);
  CHECK(levelAt(out, 48000, 2.5) < -100.0f);
}

TEST(Engine_reverseReportsLatency) {
  Rig r; CHECK(r.e.latencySamples() == 0);
  r.p.reverseOn = true; r.p.reverseWindowMs = 500; r.e.setParams(r.p);
  CHECK(r.e.latencySamples() == 24000);
  r.p.reverseOn = false; r.e.setParams(r.p);
  CHECK(r.e.latencySamples() == 0);
}

TEST(Engine_reverseDelaysDryByLatency) {
  Rig r; r.p.reverseOn = true; r.p.reverseWindowMs = 100; r.p.mix = 0; r.e.setParams(r.p); r.e.reset();
  std::vector<float> in(48000, 0.0f); in[1000] = 1.0f;
  auto out = r.run(in);
  size_t pk = 0; for (size_t i = 0; i < out.size(); ++i) if (std::fabs(out[i]) > std::fabs(out[pk])) pk = i;
  CHECK(pk == 1000 + 4800);
}

TEST(Engine_lfoTargetsAffectOutput) {
  using T = ei::EngineParams::LfoTarget;
  auto in = noiseBurst(48000, 0.5, 2.0);
  for (T t : {T::Size, T::Pitch, T::Damping, T::Pan}) {
    auto make = [&](float depth) {
      Rig r; r.p.lfoMode = ei::Lfo::Mode::Manual; r.p.lfoRateHz = 3; r.p.lfoTarget = t; r.p.lfoDepth = depth; r.p.pitchMix = 0.5f;
      r.e.setParams(r.p); r.e.reset(); return r.run(in);
    };
    auto a = make(0.0f), b = make(1.0f);
    double diff = 0; for (size_t i = 0; i < a.size(); ++i) diff += std::fabs(a[i] - b[i]);
    CHECK(diff / a.size() > 1e-4);
  }
}

TEST(Engine_noTempoFallback) {
  for (double bpm : {0.0, -5.0}) {
    Rig r; r.e.setTempo(bpm);
    r.p.lfoMode = ei::Lfo::Mode::Manual; r.p.lfoSync = true; r.p.lfoDepth = 1;
    r.p.reverseOn = true; r.p.reverseSync = true; r.p.reverseDivision = 5;      // 1/4 note = 0.5 s at 120 BPM
    r.e.setParams(r.p); r.e.reset();
    CHECK(r.e.latencySamples() == 24000);
    float peak; CHECK(allFinite(r.run(noiseBurst(48000, 1.0, 2.0)), peak));
  }
}

TEST(Engine_sampleRates) {
  ei::ReverbEngine e;
  for (double sr : {44100.0, 96000.0, 192000.0, 44100.0}) {
    e.prepare(sr, 512);
    ei::EngineParams p; p.lfoDepth = 0; p.mix = 1; p.releaseSec = 2; p.damping = 0; p.preDelayMs = 100; e.setParams(p); e.reset();
    std::vector<float> l(size_t(2.2 * sr), 0.0f); l[0] = 1.0f; auto r = l;
    for (size_t o = 0; o < l.size(); o += 512) { int n = int(std::min<size_t>(512, l.size() - o)); e.process(l.data() + o, r.data() + o, n); }
    float early = 0; for (int i = 0; i < int(0.099 * sr); ++i) early = std::fmax(early, std::fabs(l[i]));
    CHECK(early < 1e-4f);                                                   // pre-delay stays 100 ms
    double l1 = th::rmsDb(l, int(0.6 * sr), int(0.1 * sr)), l2 = th::rmsDb(l, int(1.7 * sr), int(0.1 * sr));
    CHECK_NEAR(60.0 * 1.1 / (l1 - l2), 2.0, 0.5);                           // RT60 stays 2 s
  }
}

TEST(Engine_automationSweepsAreSafe) {
  std::mt19937 rng(11); std::uniform_real_distribution<float> u(0, 1), s(-1, 1);
  ei::ReverbEngine e; e.prepare(48000, 256);
  ei::EngineParams p; bool finite = true; float peak = 0;
  std::vector<float> l(256), r(256);
  for (int b = 0; b < 20 * 48000 / 256; ++b) {
    p.size = 1 + 999 * u(rng); p.diffusion = u(rng); p.damping = s(rng); p.pitch = 12 * s(rng); p.pitchMix = u(rng); p.width = 2 * u(rng);
    p.freeze = u(rng) < 0.05f; p.preDelayMs = 500 * u(rng); p.sustainSec = 10 * u(rng) * u(rng); p.releaseSec = 0.1f + 60 * u(rng); p.releaseInfinite = u(rng) < 0.2f;
    for (auto& band : p.eq) { band.on = u(rng) < 0.8f; band.freq = 20 * std::pow(1000.0f, u(rng)); band.gainDb = 18 * s(rng); band.q = 0.1f + 9.9f * u(rng); }
    p.eqBand6HighCut = u(rng) < 0.5f;
    p.lfoMode = u(rng) < 0.5f ? ei::Lfo::Mode::Auto : ei::Lfo::Mode::Manual; p.lfoShape = ei::Lfo::Shape(int(u(rng) * 4.99f));
    p.lfoRateHz = 0.01f + 20 * u(rng); p.lfoSync = u(rng) < 0.5f; p.lfoDivision = int(u(rng) * 8.99f); p.lfoDepth = u(rng); p.lfoPhaseDeg = 360 * u(rng);
    p.lfoTarget = ei::EngineParams::LfoTarget(int(u(rng) * 3.99f));
    p.reverseOn = u(rng) < 0.5f; p.reverseWindowMs = 50 + 1950 * u(rng); p.reverseSync = u(rng) < 0.5f; p.reverseDivision = int(u(rng) * 8.99f);
    p.gateOn = u(rng) < 0.5f; p.gateThresholdDb = -80 * u(rng); p.gateAttackMs = 0.1f + 100 * u(rng); p.gateHoldMs = 2000 * u(rng); p.gateReleaseMs = 5 + 1995 * u(rng); p.gateRangeDb = -80 * u(rng);
    p.gateSource = u(rng) < 0.5f ? ei::EngineParams::GateSource::Dry : ei::EngineParams::GateSource::Wet;
    p.mix = u(rng); p.outputDb = -24 + 36 * u(rng);
    e.setTempo(u(rng) < 0.1f ? 0.0 : 40 + 200 * u(rng));
    e.setParams(p);
    for (int i = 0; i < 256; ++i) { l[i] = s(rng); r[i] = s(rng); }
    e.process(l.data(), r.data(), 256);
    for (int i = 0; i < 256; ++i) { finite = finite && std::isfinite(l[i]) && std::isfinite(r[i]); peak = std::fmax(peak, std::fmax(std::fabs(l[i]), std::fabs(r[i]))); }
  }
  CHECK(finite); CHECK(peak <= 8.0f);
}

TEST(Engine_blockSizeInvariant) {
  auto in = noiseBurst(48000, 1.0, 2.0);
  auto make = [&](int mode) {
    ei::ReverbEngine e; e.prepare(48000, 8192);
    ei::EngineParams p; p.mix = 0.5f; p.lfoMode = ei::Lfo::Mode::Manual; p.lfoDepth = 0.5f; p.lfoRateHz = 2; p.reverseOn = true; p.reverseWindowMs = 120;
    p.gateOn = true; p.sustainSec = 0.3f; p.pitch = 7; p.pitchMix = 0.3f; p.eq[3].gainDb = 6;
    e.setParams(p); e.reset();
    std::vector<float> l(in), r(in); std::mt19937 rng(5);
    for (size_t o = 0; o < l.size();) {
      int want = mode == 0 ? 1 : mode == 1 ? 64 : mode == 2 ? 480 : 1 + int(rng() % 3000);
      int n = int(std::min<size_t>(size_t(want), l.size() - o));
      e.setParams(p); e.process(l.data() + o, r.data() + o, n); o += size_t(n);
    }
    return l;
  };
  auto ref = make(1);
  for (int mode : {0, 2, 3}) {
    auto y = make(mode); double d = 0;
    for (size_t i = 0; i < y.size(); ++i) d = std::fmax(d, std::fabs(y[i] - ref[i]));
    CHECK(d < 1e-4);
  }
}

TEST(Engine_metersReportActivity) {
  Rig r; r.p.gateOn = true; r.e.setParams(r.p); r.e.reset();
  auto in = noiseBurst(48000, 1.0, 1.0);
  r.run(in);
  ei::Meters m = r.e.meters();
  CHECK(m.inputLevel > 0.1f); CHECK(m.wetLevel > 0.001f); CHECK(m.gateGain > 0.9f);
}
