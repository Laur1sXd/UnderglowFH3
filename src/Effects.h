#pragma once
// Effect patterns and palette adapted from FH2Underglow by Bartholomew Ho.
// MIT terms are in LICENSE-FH2Underglow.

#include <cmath>
#include <cstdint>
#include <cstring>

namespace underglow_effects {
struct Color {
    const char* name;
    float rgb[3];
};
// The menus' colors: these, then the ini's ten custom ones ([CustomColors]). Settings keep a color by its
// place in that list (0.1 to 0.4 had nine colors and one Custom, at 9: see load_config).
const Color kColors[] = {
    {"Cyan", {0.0f, 1.0f, 1.0f}},    {"Blue", {0.0f, 0.25f, 1.0f}},   {"Purple", {0.6f, 0.0f, 1.0f}},
    {"Pink", {1.0f, 0.1f, 0.6f}},    {"Red", {1.0f, 0.0f, 0.0f}},     {"Orange", {1.0f, 0.35f, 0.0f}},
    {"Yellow", {1.0f, 0.85f, 0.0f}}, {"Green", {0.0f, 1.0f, 0.15f}},  {"White", {1.0f, 1.0f, 1.0f}},
    {"Lime", {0.55f, 1.0f, 0.0f}},   {"Teal", {0.0f, 0.75f, 0.6f}},   {"Sky Blue", {0.3f, 0.7f, 1.0f}},
    {"Indigo", {0.3f, 0.0f, 1.0f}},  {"Magenta", {1.0f, 0.0f, 1.0f}}, {"Crimson", {1.0f, 0.0f, 0.25f}},
    {"Amber", {1.0f, 0.6f, 0.0f}},   {"Mint", {0.4f, 1.0f, 0.6f}},    {"Lavender", {0.7f, 0.5f, 1.0f}},
};
const int kColorCount = int(sizeof kColors / sizeof kColors[0]);   // the preset colors
const int kCustomCount = 10;
const int kColorTotal = kColorCount + kCustomCount;                 // with the custom ones after them
const float kCustomDefaults[kCustomCount][3] = {
    {0.0f, 1.0f, 1.0f}, {1.0f, 0.5f, 0.5f}, {1.0f, 0.8f, 0.5f}, {1.0f, 1.0f, 0.5f}, {0.5f, 1.0f, 0.5f},
    {0.5f, 1.0f, 1.0f}, {0.5f, 0.6f, 1.0f}, {0.8f, 0.5f, 1.0f}, {1.0f, 0.5f, 1.0f}, {1.0f, 0.9f, 0.8f},
};

enum Effect {
    kEffectStatic, kEffectRainbow, kEffectWave, kEffectBreathe, kEffectWipe, kEffectStack, kEffectBlink,
    kEffectPolice, kEffectAudio, kEffectCrossing, kEffectHypnotoad, kEffectLightning, kEffectMarquee,
    kEffectComet, kEffectFill, kEffectSwap, kEffectVisor,
    // 0.4: more that follow the radio. New effects go on the end: the ini keeps effects by number.
    kEffectAudioPulse, kEffectAudioPeak, kEffectAudioBlend, kEffectBassFlash, kEffectAudioSparkle,
    kEffectAudioRainbow, kEffectAudioCenter,
    // 0.5: after OpenRGB Effects Plugin's effects (written afresh, for a ring of lights round a car)
    kEffectAudioBubbles, kEffectAudioParty, kEffectAudioSine, kEffectAudioStar, kEffectAudioSync, kEffectVuMeter,
    kEffectAudioSwirl,
    kEffectBouncingBall, kEffectBreathingCircle, kEffectFractalMotion, kEffectMotionPoint, kEffectRain,
    kEffectRotatingBeam, kEffectSmoothBlink, kEffectSparkleFade, kEffectZigZag, kEffectMarquee1,
    kEffectAlternateBlink, kEffectGradientWave, kEffectMovingPanes, kEffectNoiseMap, kEffectSequence, kEffectWavy,
    kEffectTwoColorMarquee,
    kEffectBloom, kEffectBubbles, kEffectColorWheel, kEffectDoubleRainbow, kEffectMotionPoints, kEffectMosaic,
    kEffectRadialRainbow, kEffectRandomSpin, kEffectRotatingRainbow, kEffectSpiral, kEffectStarryNight,
    kEffectSunrise, kEffectSwirlCircles,
    kEffectCount
};
const char* const kEffects[kEffectCount] = {
    "Static", "Rainbow", "Rainbow Wave", "Breathe", "Wipe", "Stack", "Blink", "Police", "Audio Visualizer",
    "Crossing Beams", "Hypnotoad", "Lightning", "Random Marquee", "Comet", "Fill", "Swap", "Visor",
    "Audio Pulse", "Peak Meter", "Audio Blend", "Bass Flash", "Audio Sparkle", "Audio Rainbow", "Center Burst",
    "Audio Bubbles", "Audio Party", "Audio Sine", "Audio Star", "Audio Sync", "VU Meter", "Audio Swirl",
    "Bouncing Ball", "Breathing Circle", "Fractal Motion", "Motion Point", "Rain", "Rotating Beam", "Smooth Blink",
    "Sparkle Fade", "Zig Zag", "Marquee",
    "Alternate Blink", "Gradient Wave", "Moving Panes", "Noise Map", "Sequence", "Wavy", "Two-Color Marquee",
    "Bloom", "Bubbles", "Color Wheel", "Double Rainbow", "Motion Points", "Mosaic", "Radial Rainbow", "Random Spin",
    "Rotating Rainbow", "Spiral", "Starry Night", "Sunrise", "Swirl Circles",
};

// The effects in groups, for the menus (ANNA's submenus, the Mods page's list in this order).
enum EffectGroup { kGroupAudio, kGroupOneColor, kGroupTwoColors, kGroupNoColor, kGroupCount };
const char* const kGroupNames[kGroupCount] = {"Audio Visualizer", "Effects with 1 Color", "Effects with 2 Colors",
                                              "Multicolor Effects"};
const char* const kGroupKeys[kGroupCount] = {"audio", "one", "two", "none"};
struct GroupedEffect {
    int group, effect;
};
const GroupedEffect kEffectOrder[] = {
    {kGroupAudio, kEffectAudio}, {kGroupAudio, kEffectAudioPulse}, {kGroupAudio, kEffectAudioPeak},
    {kGroupAudio, kEffectAudioBlend}, {kGroupAudio, kEffectBassFlash}, {kGroupAudio, kEffectAudioSparkle},
    {kGroupAudio, kEffectAudioCenter}, {kGroupAudio, kEffectAudioRainbow}, {kGroupAudio, kEffectVuMeter},
    {kGroupAudio, kEffectAudioSine}, {kGroupAudio, kEffectAudioSync}, {kGroupAudio, kEffectAudioParty},
    {kGroupAudio, kEffectAudioStar}, {kGroupAudio, kEffectAudioBubbles}, {kGroupAudio, kEffectAudioSwirl},
    {kGroupOneColor, kEffectStatic}, {kGroupOneColor, kEffectBreathe}, {kGroupOneColor, kEffectSmoothBlink},
    {kGroupOneColor, kEffectBlink}, {kGroupOneColor, kEffectWipe}, {kGroupOneColor, kEffectFill},
    {kGroupOneColor, kEffectStack}, {kGroupOneColor, kEffectComet}, {kGroupOneColor, kEffectVisor},
    {kGroupOneColor, kEffectMarquee1}, {kGroupOneColor, kEffectMotionPoint}, {kGroupOneColor, kEffectBouncingBall},
    {kGroupOneColor, kEffectRain}, {kGroupOneColor, kEffectSparkleFade}, {kGroupOneColor, kEffectRotatingBeam},
    {kGroupOneColor, kEffectBreathingCircle}, {kGroupOneColor, kEffectZigZag},
    {kGroupOneColor, kEffectFractalMotion}, {kGroupOneColor, kEffectLightning},
    {kGroupTwoColors, kEffectCrossing}, {kGroupTwoColors, kEffectSwap}, {kGroupTwoColors, kEffectAlternateBlink},
    {kGroupTwoColors, kEffectGradientWave}, {kGroupTwoColors, kEffectSequence}, {kGroupTwoColors, kEffectWavy},
    {kGroupTwoColors, kEffectTwoColorMarquee}, {kGroupTwoColors, kEffectMovingPanes},
    {kGroupTwoColors, kEffectNoiseMap},
    {kGroupNoColor, kEffectRainbow}, {kGroupNoColor, kEffectWave}, {kGroupNoColor, kEffectColorWheel},
    {kGroupNoColor, kEffectRotatingRainbow}, {kGroupNoColor, kEffectDoubleRainbow},
    {kGroupNoColor, kEffectRadialRainbow}, {kGroupNoColor, kEffectSpiral}, {kGroupNoColor, kEffectRandomSpin},
    {kGroupNoColor, kEffectHypnotoad}, {kGroupNoColor, kEffectSwirlCircles}, {kGroupNoColor, kEffectMotionPoints},
    {kGroupNoColor, kEffectMarquee}, {kGroupNoColor, kEffectMosaic}, {kGroupNoColor, kEffectStarryNight},
    {kGroupNoColor, kEffectBubbles}, {kGroupNoColor, kEffectBloom}, {kGroupNoColor, kEffectSunrise},
    {kGroupNoColor, kEffectPolice},
};
static_assert(sizeof kEffectOrder / sizeof kEffectOrder[0] == kEffectCount, "every effect in a group, once");
enum Zone { kZoneFront, kZoneRight, kZoneRear, kZoneLeft, kZoneCount };

// A fully saturated color at hue h (0..1: red, yellow, green, cyan, blue, magenta, red).
void hue_rgb(float h, float out[3]) {
    h = (h - std::floor(h)) * 6.0f;
    const float x = 1.0f - std::fabs(std::fmod(h, 2.0f) - 1.0f);
    const int sector = int(h) % 6;
    const float table[6][3] = {{1, x, 0}, {x, 1, 0}, {0, 1, x}, {0, x, 1}, {x, 0, 1}, {1, 0, x}};
    memcpy(out, table[sector], 12);
}

// Where light i of the wheel rectangle is, for the effects: its zone, and its place along that zone counted the
// same way on both sides of the car (front and rear: left to right; the sides: front to back).
struct Pixel {
    int zone = 0;
    int li = 0, ln = 1;       // index and count in its zone
    float t = 0.5f;           // (li + 0.5) / ln
    float lateral = 0.0f;     // -1 the car's left .. +1 its right
    int gi = 0, gn = 1;       // index and count along every zone playing the same effect, round the car
    float ring = 0.5f;        // (gi + 0.5) / gn
    // Where it is on the car, for the effects drawn on it as a picture (a wheel, a spiral, bubbles...):
    float x = 0.0f, y = 0.0f; // -1 the left .. +1 the right; +1 the front .. -1 the back
    float ang = 0.0f;         // round the car from the middle: 0 the front, 0.25 the right, 0.5 the back, 0.75 the left
    float rd = 0.0f;          // 0 the middle of the sides .. 1 the front and back
    int id = 0;               // the light's own number, for the random ones
};

Pixel pixel(int zone, int index, int count) {
    Pixel result;
    auto* p = &result;
    const int e = zone;
    p->zone = e;
    p->ln = count;
    // edge_point runs the rear right to left and the left side back to front: turn those round.
    p->li = e == kZoneRear || e == kZoneLeft ? p->ln - 1 - index : index;
    p->t = (float(p->li) + 0.5f) / float(p->ln);
    p->lateral = e == kZoneRight ? 1.0f : e == kZoneLeft ? -1.0f : 2.0f * p->t - 1.0f;
    p->x = p->lateral;
    p->y = e == kZoneFront ? 1.0f : e == kZoneRear ? -1.0f : 1.0f - 2.0f * p->t;
    p->ang = float(std::atan2(double(p->x), double(p->y)) / 6.283185307179586);
    if (p->ang < 0.0f) p->ang += 1.0f;
    p->rd = std::sqrt(0.16f * p->x * p->x + p->y * p->y) / 1.0770330f;
    if (p->rd > 1.0f) p->rd = 1.0f;
    return result;
}

double frac(double x) { return x - std::floor(x); }
float clamp01(float x) { return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x; }
float tri(double x) { return float(1.0 - std::fabs(2.0 * frac(x) - 1.0)); }   // 0 -> 1 -> 0 over a cycle

float hash01(uint32_t x) {   // a repeatable "random" number in 0..1
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return float(x & 0xFFFFFF) / float(0x1000000);
}

// The music, once a frame, for the audio effects: each zone's level (0..1) and a peak that holds, then falls.
struct AudioFrame {
    float lvl[kZoneCount] = {};
    float peak[kZoneCount] = {};
    uint64_t peak_at[kZoneCount] = {};
    uint64_t last = 0;
};

struct EffectFrame {
    float speed;
    float primary[3];
    float secondary[3];
    const AudioFrame& audio;
};

bool silent(float lvl) { return lvl <= 0.03f; }

// How far apart two places round the car are (0..0.5 of the way round).
float adist(float a, float b) { return std::fabs(float(frac(double(a) - double(b) + 0.5)) - 0.5f); }
float smooth01(float x) {
    x = clamp01(x);
    return x * x * (3.0f - 2.0f * x);
}

void mix_rgb(const float a[3], const float b[3], float t, float out[3]) {
    for (int k = 0; k < 3; ++k) out[k] = a[k] + (b[k] - a[k]) * t;
}

// A soft dot at `pos` (0..1 along the zone), `width` lights wide each way.
float dot(const Pixel& p, float pos, float width) {
    const float d = std::fabs((float(p.li) + 0.5f) - pos * float(p.ln));
    return clamp01(1.0f - d / width);
}

// Light p's color and intensity (0..1 of the brightness) now, playing `effect`.
void effect_now(const EffectFrame& c, int effect, const Pixel& p, double secs, float rgb[3], float* level) {
    const double phase = secs * c.speed;
    const double kTau = 6.283185307179586;
    float base[3], other[3];
    memcpy(base, c.primary, sizeof base);
    memcpy(other, c.secondary, sizeof other);
    memcpy(rgb, base, 12);
    *level = 1.0f;
    switch (effect) {
    case kEffectRainbow:
        hue_rgb(float(phase), rgb);
        break;
    case kEffectWave:
        hue_rgb(float(phase + p.ring), rgb);
        break;
    case kEffectBreathe:
        *level = float(0.1 + 0.9 * (0.5 - 0.5 * std::cos(phase * kTau)));
        break;
    case kEffectWipe: {   // the color wipes on along the zone, then off the same way
        const int step = int(frac(phase) * 2.0 * p.ln);
        *level = step < p.ln ? (p.li <= step ? 1.0f : 0.0f) : (p.li > step - p.ln ? 1.0f : 0.0f);
        break;
    }
    case kEffectStack: {   // lights drop one at a time from the start and pile up at the end
        const int n = p.ln;
        const int moves = n * (n + 1) / 2, hold = n > 2 ? n : 2;
        int step = int(frac(phase * 0.5) * double(moves + hold));
        int stacked = 0;
        while (stacked < n && step >= n - stacked) step -= n - stacked++;
        *level = p.li >= n - stacked || (stacked < n && p.li == step) ? 1.0f : 0.0f;
        break;
    }
    case kEffectBlink:
        *level = frac(phase * 5.0) < 0.5 ? 1.0f : 0.0f;
        break;
    case kEffectPolice: {   // left red, right blue, a double flash each in turn
        const double q = frac(phase * 3.0);
        const bool left = p.lateral <= 0.0f;
        const float red[3] = {1.0f, 0.0f, 0.0f}, blue[3] = {0.0f, 0.15f, 1.0f};
        memcpy(rgb, left ? red : blue, 12);
        const double h = frac(q * 2.0);
        const bool turn = left == (q < 0.5);
        *level = turn && (h < 0.2 || (h > 0.3 && h < 0.5)) ? 1.0f : 0.0f;
        break;
    }
    case kEffectAudio: {   // a level meter from the middle of each zone outwards, green to red
        const float lvl = c.audio.lvl[p.zone];
        const float d = std::fabs(2.0f * p.t - 1.0f);   // 0 the middle, 1 the ends
        hue_rgb((1.0f - d) / 3.0f, rgb);                  // green in the middle, red at the ends
        *level = !silent(lvl) ? clamp01((lvl - d) * float(p.ln) * 0.5f + 1.0f) : 0.0f;   // silence: dark
        break;
    }
    case kEffectAudioPulse: {   // the whole zone in the primary color, as bright as its band is loud
        const float lvl = c.audio.lvl[p.zone];
        *level = silent(lvl) ? 0.0f : lvl;
        break;
    }
    case kEffectAudioPeak: {   // a bar up the zone in the primary color, a falling peak light in the secondary
        const float lvl = c.audio.lvl[p.zone], pk = c.audio.peak[p.zone];
        if (silent(pk)) {
            *level = 0.0f;
            break;
        }
        int top = int(pk * float(p.ln));
        if (top > p.ln - 1) top = p.ln - 1;
        if (p.li == top) {
            memcpy(rgb, other, 12);
            *level = 1.0f;
        } else {
            *level = silent(lvl) ? 0.0f : clamp01(lvl * float(p.ln) - float(p.li));
        }
        break;
    }
    case kEffectAudioBlend: {   // quiet: the primary color, dim; loud: the secondary, bright
        const float lvl = c.audio.lvl[p.zone];
        mix_rgb(base, other, lvl, rgb);
        *level = silent(lvl) ? 0.0f : 0.2f + 0.8f * lvl;
        break;
    }
    case kEffectBassFlash: {   // the whole car flashes on the bass, primary; the hardest hits in the secondary
        const float bass = c.audio.lvl[kZoneRear];
        if (bass > 0.85f) memcpy(rgb, other, 12);
        *level = silent(bass) ? 0.0f : bass * bass;
        break;
    }
    case kEffectAudioSparkle: {   // lights twinkle at random in the primary color, more of them the louder it is
        const float lvl = c.audio.lvl[p.zone];
        const double slots = secs * 12.0;
        const uint32_t slot = uint32_t(int64_t(slots));
        const float f = float(frac(slots));
        const float r = hash01(slot * 131u + uint32_t(p.gi) * 7u + uint32_t(p.zone));
        *level = !silent(lvl) && r < lvl * 0.7f ? (1.0f - f) * (1.0f - f) : 0.0f;
        break;
    }
    case kEffectAudioCenter: {   // from the middle outwards with the music, primary in the middle to secondary
        const float lvl = c.audio.lvl[p.zone];
        const float d = std::fabs(2.0f * p.t - 1.0f);
        mix_rgb(base, other, d, rgb);
        *level = !silent(lvl) ? clamp01((lvl - d) * float(p.ln) * 0.5f + 1.0f) : 0.0f;
        break;
    }
    case kEffectAudioRainbow: {   // a rainbow running round, each zone as bright as its band; the beat pushes the hue
        const float lvl = c.audio.lvl[p.zone];
        hue_rgb(float(phase + p.ring) + 0.3f * lvl, rgb);
        *level = silent(lvl) ? 0.0f : 0.15f + 0.85f * lvl;
        break;
    }
    case kEffectCrossing: {   // two beams from opposite ends, crossing in the middle
        const float pos = tri(phase * 1.5);
        const float a = dot(p, pos, 1.5f), b = dot(p, 1.0f - pos, 1.5f);
        const float sum = a + b > 0.0f ? a + b : 1.0f;
        for (int k = 0; k < 3; ++k) rgb[k] = (base[k] * a + other[k] * b) / sum;
        *level = a > b ? a : b;
        break;
    }
    case kEffectHypnotoad: {   // rainbow rings running out from the middle, pulsing
        const float d = std::fabs(2.0f * p.t - 1.0f);
        hue_rgb(float(d * 1.5 - phase * 2.0), rgb);
        *level = float(0.25 + 0.75 * (0.5 + 0.5 * std::sin(phase * kTau * 3.0 - d * 6.0)));
        break;
    }
    case kEffectLightning: {   // now and then, a few quick flashes on a random stretch
        const double slots = secs * (0.5 + 2.0 * c.speed);
        const uint32_t slot = uint32_t(int64_t(slots)) * 4u + uint32_t(p.zone);
        const double into = frac(slots) / (0.5 + 2.0 * c.speed);   // seconds into the slot
        *level = 0.0f;
        if (hash01(slot * 7u + 1u) > 0.55f) break;   // no strike this time
        const double start = hash01(slot * 7u + 2u) * 0.4;
        const int flashes = 1 + int(hash01(slot * 7u + 3u) * 3.0f);
        const int from = int(hash01(slot * 7u + 4u) * float(p.ln));
        const int len = 1 + int(hash01(slot * 7u + 5u) * float(p.ln));
        if (p.li < from || p.li >= from + len) break;
        for (int f = 0; f < flashes; ++f) {
            const double t0 = start + f * 0.1;
            if (into >= t0 && into < t0 + 0.05) *level = f == 0 ? 1.0f : 0.6f;
        }
        break;
    }
    case kEffectMarquee: {   // every third light on, running round, each a random color
        const int64_t step = int64_t(phase * 20.0);
        const int64_t k = int64_t(p.gi) - step;
        const int64_t m = ((k % 3) + 3) % 3;
        const int64_t group = (k - m) / 3;
        hue_rgb(hash01(uint32_t(group)), rgb);
        *level = m == 0 ? 1.0f : 0.0f;
        break;
    }
    case kEffectComet: {   // a bright head running round the car, a fading tail behind it
        const double head = frac(phase * 1.5);
        const double behind = frac(head - p.ring);
        const double tail = std::fmax(0.3, 4.0 / double(p.gn));
        *level = behind < tail ? float((1.0 - behind / tail) * (1.0 - behind / tail)) : 0.0f;
        break;
    }
    case kEffectFill: {   // the zone fills up from its start, then empties back
        const float lvl = tri(phase);
        *level = clamp01(lvl * float(p.ln) - float(p.li));
        break;
    }
    case kEffectSwap: {   // half one color, half the other, swapping over
        const bool first = p.t < 0.5f;
        const bool swapped = int64_t(phase * 2.0) % 2 != 0;
        if (first == swapped) memcpy(rgb, other, 12);
        break;
    }
    case kEffectVisor: {   // one light sweeping back and forth, scanner style
        const float pos = float(0.5 - 0.5 * std::cos(phase * 1.5 * kTau));
        const float w = std::fmax(1.5f, float(p.ln) * 0.2f);
        const float v = dot(p, pos, w);
        *level = v * v;
        break;
    }

    // ---- 0.5: effects after OpenRGB Effects Plugin's, written afresh for a ring of lights round a car ----
    // The music: each part's band (g_audio), as for the audio effects above.
    case kEffectAudioBubbles: {   // rings of color pop up round the car with the music and spread
        const float lvl = c.audio.lvl[p.zone];
        *level = 0.0f;
        if (silent(lvl)) break;
        const double slots = secs * 5.0;
        const int64_t now = int64_t(std::floor(slots));
        for (int64_t k = now; k > now - 4; --k) {
            const uint32_t id = uint32_t(k);
            const float age = float((slots - double(k)) / 4.0);
            const float r = 0.02f + age * 0.15f * (0.5f + lvl);
            const float v = clamp01(1.0f - std::fabs(adist(p.ang, hash01(id * 13u + 1u)) - r) / 0.035f) * (1.0f - age);
            if (v > *level) {
                *level = v;
                hue_rgb(hash01(id * 13u + 2u), rgb);
            }
        }
        *level = clamp01(*level * (0.4f + lvl));
        break;
    }
    case kEffectAudioParty: {   // each part flashes with its band, in a color that keeps changing
        const float lvl = c.audio.lvl[p.zone];
        hue_rgb(hash01(uint32_t(int64_t(secs * 3.0)) * 4u + uint32_t(p.zone)), rgb);
        *level = silent(lvl) ? 0.0f : lvl * lvl;
        break;
    }
    case kEffectAudioSine: {   // a sine wave running along each part, as high as the music is loud
        const float lvl = c.audio.lvl[p.zone];
        const float w = float(0.5 + 0.5 * std::sin((p.t * 2.0 - phase * 2.0) * kTau));
        *level = silent(lvl) ? 0.0f : clamp01(w * lvl * 1.6f);
        break;
    }
    case kEffectAudioStar: {   // colored stars come out with the music, more of them the louder, and fade
        const float lvl = c.audio.lvl[p.zone];
        *level = 0.0f;
        if (silent(lvl)) break;
        const double slots = secs * 4.0;
        const int64_t now = int64_t(std::floor(slots));
        for (int64_t k = now; k > now - 2; --k) {
            const uint32_t h = uint32_t(k) * 97u + uint32_t(p.id) * 31u;
            if (hash01(h + 5u) >= lvl * 0.5f) continue;
            const float v = 1.0f - float((slots - double(k)) / 2.0);
            if (v > *level) {
                *level = v;
                hue_rgb(hash01(h + 6u), rgb);
            }
        }
        break;
    }
    case kEffectAudioSync: {   // the whole car one color: red with the bass, through yellow and green, blue with treble
        const float b = c.audio.lvl[kZoneRear], lm = c.audio.lvl[kZoneLeft], hm = c.audio.lvl[kZoneRight],
                    tr = c.audio.lvl[kZoneFront];
        const float sum = b + lm + hm + tr, loud = std::fmax(std::fmax(b, lm), std::fmax(hm, tr));
        hue_rgb(sum > 0.0f ? (lm * 0.15f + hm * 0.4f + tr * 0.65f) / sum : 0.0f, rgb);
        *level = silent(loud) ? 0.0f : loud;
        break;
    }
    case kEffectVuMeter: {   // a bar up each part with its band: the primary color, to the secondary at the top
        const float lvl = c.audio.lvl[p.zone];
        mix_rgb(base, other, p.t, rgb);
        *level = silent(lvl) ? 0.0f : clamp01(lvl * float(p.ln) - float(p.li));
        break;
    }
    case kEffectAudioSwirl: {   // two lights, the two colors, chasing round the car, as big as the music is loud
        const float loud = (c.audio.lvl[0] + c.audio.lvl[1] + c.audio.lvl[2] + c.audio.lvl[3]) * 0.25f;
        const float a = float(frac(phase * 0.7)), b = float(frac(phase * 0.7 + 0.5));
        const float w = 0.04f + 0.2f * loud;
        const float va = clamp01(1.0f - adist(p.ang, a) / w), vb = clamp01(1.0f - adist(p.ang, b) / w);
        memcpy(rgb, va >= vb ? base : other, 12);   // one in the primary color, one in the secondary
        *level = silent(loud) ? 0.0f : std::fmax(va, vb);
        break;
    }
    // One color: the primary.
    case kEffectBouncingBall: {   // a ball bouncing along each part, slowing at the top of each bounce
        const double u = frac(phase * 1.5);
        *level = dot(p, float(4.0 * u * (1.0 - u)), 1.3f);
        break;
    }
    case kEffectBreathingCircle: {   // a ring of light growing out from the middle of the car and back
        const float r = float(0.5 - 0.5 * std::cos(phase * kTau));
        *level = clamp01(1.0f - std::fabs(p.rd - r) / 0.25f);
        break;
    }
    case kEffectFractalMotion: {   // waves over waves of light, never quite the same twice
        const double a = p.ang * kTau, q = phase * 3.0;
        const double v = std::sin(a * 2.0 + q * 1.1) * 0.5 + std::sin(a * 5.0 - q * 1.7) * 0.3 +
                         std::sin(a * 9.0 + q * 2.3) * 0.2;
        *level = clamp01(float(0.5 + 0.6 * v));
        break;
    }
    case kEffectMotionPoint: {   // one light gliding round the car, easing to a stop, and back
        const float pos = float(0.5 - 0.5 * std::cos(phase * 0.5 * kTau));
        *level = clamp01(1.0f - adist(p.ang, pos) / 0.05f);
        break;
    }
    case kEffectRain: {   // drops running along each part, front to back down the sides
        *level = 0.0f;
        for (uint32_t k = 0; k < 3; ++k) {
            const uint32_t d = uint32_t(p.zone) * 3u + k;
            const double run = phase * (1.5 + hash01(d * 7u + 1u)) + hash01(d * 7u + 2u);
            const float behind = float(frac(run)) * 1.3f - p.t;   // a gap off the end between drops
            if (behind >= 0.0f && behind < 0.25f) *level = std::fmax(*level, 1.0f - behind / 0.25f);
        }
        break;
    }
    case kEffectRotatingBeam: {   // a beam through the middle of the car, turning
        const float a = float(frac(phase * 0.5));
        *level = clamp01(1.0f - std::fmin(adist(p.ang, a), adist(p.ang, a + 0.5f)) / 0.07f);
        break;
    }
    case kEffectSmoothBlink: {   // on, a soft fade out, off, a soft fade in
        const float u = float(frac(phase * 2.0));
        *level = u < 0.4f ? 1.0f : u < 0.5f ? smooth01((0.5f - u) * 10.0f) : u < 0.9f ? 0.0f : smooth01((u - 0.9f) * 10.0f);
        break;
    }
    case kEffectSparkleFade: {   // lights flick on here and there and fade away
        const double slots = phase * 8.0 + hash01(uint32_t(p.id) * 3u + 1u);
        const float f = float(frac(slots));
        *level = hash01(uint32_t(int64_t(slots)) * 131u + uint32_t(p.id) * 17u) < 0.3f ? (1.0f - f) * (1.0f - f) : 0.0f;
        break;
    }
    case kEffectZigZag: {   // a light zigzagging from side to side down the car
        const double u = frac(phase * 0.5);
        const float py = float(1.0 - 2.0 * u), qx = 2.0f * tri(u * 3.0) - 1.0f;
        const float d = std::sqrt((p.x - qx) * (p.x - qx) + (p.y - py) * (p.y - py));
        *level = clamp01(1.0f - d / 0.35f);
        break;
    }
    case kEffectMarquee1: {   // every third light on, running round
        const int64_t k = int64_t(p.gi) - int64_t(phase * 20.0);
        *level = ((k % 3) + 3) % 3 == 0 ? 1.0f : 0.0f;
        break;
    }
    // Two colors: the primary and the secondary.
    case kEffectAlternateBlink: {   // blinks in the primary color, then in the secondary
        const double u = frac(phase * 2.5);
        if (u >= 0.5) memcpy(rgb, other, 12);
        *level = frac(u * 2.0) < 0.6 ? 1.0f : 0.0f;
        break;
    }
    case kEffectGradientWave: {   // the primary color fading into the secondary and back, running round
        mix_rgb(base, other, float(0.5 - 0.5 * std::cos((p.ring - phase) * kTau)), rgb);
        break;
    }
    case kEffectMovingPanes: {   // panes of the two colors, neighboring parts sliding opposite ways
        const double dir = p.zone == kZoneFront || p.zone == kZoneRight ? 1.0 : -1.0;
        const int64_t pane = int64_t(std::floor(p.t * 4.0 + dir * phase * 2.0));
        if (((pane % 2) + 2) % 2) memcpy(rgb, other, 12);
        break;
    }
    case kEffectNoiseMap: {   // the two colors drifting into each other in slow clouds
        const double a = p.ang * kTau, q = phase * 3.0;
        const double n = std::sin(a * 2.0 + q) * 0.6 + std::sin(a * 3.0 - q * 0.7 + 1.3) * 0.4;
        mix_rgb(base, other, smooth01(float(0.5 + 0.5 * n)), rgb);
        break;
    }
    case kEffectSequence: {   // the whole car in the primary color, then the secondary, fading over
        const float u = float(frac(phase));
        const float m = u < 0.4f ? 0.0f : u < 0.5f ? smooth01((u - 0.4f) * 10.0f) : u < 0.9f ? 1.0f : smooth01((1.0f - u) * 10.0f);
        mix_rgb(base, other, m, rgb);
        break;
    }
    case kEffectWavy: {   // a standing wave, each light swinging between the two colors
        const double w = std::sin(p.t * 2.0 * kTau) * std::sin(phase * kTau);
        mix_rgb(base, other, float(0.5 + 0.5 * w), rgb);
        break;
    }
    case kEffectTwoColorMarquee: {   // pairs of lights, the two colors in turn, running round
        const int64_t k = int64_t(p.gi) - int64_t(phase * 20.0);
        const int64_t g = int64_t(std::floor(double(k) / 3.0));
        if (g % 2 != 0) memcpy(rgb, other, 12);
        *level = ((k % 3) + 3) % 3 != 2 ? 1.0f : 0.0f;
        break;
    }
    // No colors: their own.
    case kEffectBloom:     // flowers of color opening round the car and fading
    case kEffectBubbles: {   // rings of color spreading round the car
        const bool ring = effect == kEffectBubbles;
        *level = 0.0f;
        const double slots = phase * 3.0;
        const int64_t now = int64_t(std::floor(slots));
        for (int64_t k = now; k > now - 3; --k) {
            const uint32_t id = uint32_t(k) * (ring ? 7u : 11u);
            const float age = float((slots - double(k)) / 3.0);
            const float d = adist(p.ang, hash01(id + 1u)), r = 0.02f + age * 0.15f;
            const float v = ring ? clamp01(1.0f - std::fabs(d - r) / 0.03f) * (1.0f - age)
                                 : d < r ? (1.0f - age) * (1.0f - 0.5f * d / r) : 0.0f;
            if (v > *level) {
                *level = v;
                hue_rgb(hash01(id + 2u) + (ring ? 0.0f : d * 2.0f), rgb);
            }
        }
        break;
    }
    case kEffectColorWheel:   // the color wheel round the car, turning
        hue_rgb(p.ang + float(phase), rgb);
        break;
    case kEffectRotatingRainbow: {   // a rainbow straight across the car, turning round
        const double a = phase * 0.5 * kTau;
        hue_rgb(float((p.x * std::cos(a) + p.y * std::sin(a)) * 0.35), rgb);
        break;
    }
    case kEffectDoubleRainbow:   // two rainbows turning opposite ways, meeting at the front and the back
        hue_rgb((p.x >= 0.0f ? 2.0f * p.ang : -2.0f * p.ang) + float(phase), rgb);
        break;
    case kEffectRadialRainbow:   // rainbow rings running out from the middle of the car
        hue_rgb(p.rd - float(phase), rgb);
        break;
    case kEffectSpiral:   // a rainbow spiral, turning
        hue_rgb(p.ang + p.rd * 1.5f - float(phase), rgb);
        break;
    case kEffectRandomSpin: {   // each part's rainbow spinning, its speed and way changing every few seconds
        const uint32_t k = uint32_t(int64_t(phase * 0.8)) * 4u + uint32_t(p.zone);
        hue_rgb(p.t + float(phase * (hash01(k * 3u + 1u) * 2.0f - 1.0f) * 3.0), rgb);
        break;
    }
    case kEffectSwirlCircles: {   // two colors chasing each other round the car
        const float a = float(frac(phase * 0.6)), b = float(frac(phase * 0.6 + 0.5));
        const float va = clamp01(1.0f - adist(p.ang, a) / 0.2f), vb = clamp01(1.0f - adist(p.ang, b) / 0.2f);
        hue_rgb(float(phase * 0.15) + (va >= vb ? 0.0f : 0.5f), rgb);
        *level = std::fmax(va, vb);
        break;
    }
    case kEffectMotionPoints: {   // several lights gliding round, each its own color, speed and way
        *level = 0.0f;
        for (uint32_t k = 0; k < 5; ++k) {
            const float sp = (0.3f + 0.7f * hash01(k * 5u + 1u)) * (hash01(k * 5u + 4u) < 0.5f ? -1.0f : 1.0f);
            const float v = clamp01(1.0f - adist(p.ang, float(frac(phase * sp + hash01(k * 5u + 2u)))) / 0.04f);
            if (v > *level) {
                *level = v;
                hue_rgb(hash01(k * 5u + 3u), rgb);
            }
        }
        break;
    }
    case kEffectMosaic: {   // tiles of two lights, each fading over to a new color now and then
        const uint32_t tile = uint32_t(p.id / 2);
        const double run = phase * 1.5 + hash01(tile * 7u + 1u) * 4.0;
        hue_rgb(hash01(uint32_t(int64_t(run)) * 101u + tile * 7u), rgb);
        *level = float(std::sin(frac(run) * 3.141592653589793));
        break;
    }
    case kEffectStarryNight: {   // stars coming out and fading, here and there, each its own color
        const double run = phase * 1.2 + hash01(uint32_t(p.id) * 9u + 1u) * 7.0;
        const uint32_t h = uint32_t(int64_t(run)) * 61u + uint32_t(p.id) * 13u;
        hue_rgb(hash01(h + 2u), rgb);
        *level = hash01(h) < 0.45f ? float(std::sin(frac(run) * 3.141592653589793)) : 0.0f;
        break;
    }
    case kEffectSunrise: {   // a deep red glow from the middle, warming to gold and white, then again
        const float u = float(frac(phase * 0.25));
        const float glow = clamp01(u * 1.6f - p.rd * 0.6f);
        hue_rgb(0.0f + 0.13f * glow, rgb);
        const float white = clamp01((glow - 0.7f) / 0.3f) * 0.7f;
        for (int k = 0; k < 3; ++k) rgb[k] += (1.0f - rgb[k]) * white;
        *level = u > 0.92f ? glow * (1.0f - u) / 0.08f : glow;
        break;
    }
    default:
        break;
    }
}
} // namespace underglow_effects
