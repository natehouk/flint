// Flint — custom vector UI (DPF/NanoVG). Original design. License: MIT.
#include "DistrhoUI.hpp"
#include "ParamDefs.hpp"
#include "Presets.hpp"
#include "UserPresets.hpp"
#include "ResizeHandle.hpp"
#include "Help.hpp"
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32)
extern "C" __declspec(dllimport) void* __stdcall ShellExecuteA(void*, const char*, const char*, const char*, const char*, int);
#endif

START_NAMESPACE_DISTRHO

// open a web page in the default browser (the full manual)
static void openURL(const char* url) {
#if defined(_WIN32)
    ShellExecuteA(nullptr, "open", url, nullptr, nullptr, 1);
#elif defined(__APPLE__)
    const std::string c = std::string("open '") + url + "' &"; if (std::system(c.c_str())) {}
#else
    const std::string c = std::string("xdg-open '") + url + "' >/dev/null 2>&1 &"; if (std::system(c.c_str())) {}
#endif
}
static bool gTipsOn = true;          // hover tips on/off, kept for the session
static double nowSec() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

using namespace flint;
using DGL_NAMESPACE::ResizeHandle;

static constexpr float W = 900.f, H = 600.f;

#define C_BG        Color(0x0f, 0x0f, 0x10)
#define C_PANEL     Color(0x18, 0x18, 0x1a)
#define C_PANEL_HI  Color(0x24, 0x24, 0x27)
#define C_LINE      Color(0x30, 0x30, 0x34)
#define C_TEXT      Color(0xe8, 0xe6, 0xe3)
#define C_DIM       Color(0x8c, 0x8a, 0x86)
#define C_FAINT     Color(0x4e, 0x4d, 0x4b)
#define C_ACC       Color(0xf5, 0xa6, 0x3f)       // spark
#define C_EF        Color(0x86, 0xb6, 0xf0)       // closing (downward) modulation
#define C_DISPLAY   Color(0x0a, 0x0a, 0x0b)

static Color withAlpha(const Color& c, float a) { Color r(c); r.alpha = a; return r; }

// ---------------------------------------------------------------- layout
enum WType { Knob, Segment };
struct Ctl { WType type; uint32_t param; float x, y, r; const char* label; };    // Segment: r = width
struct Panel { float x, y, w, h; const char* title; };

static constexpr float DX = 16, DY = 64, DW = 868, DH = 296;          // filter display
static constexpr float PY = 372, PH = 212;                             // panels
static constexpr float SEG_H = 26;
static constexpr float LIGHT_X = 676, LIGHT_Y = PY + 28;               // envelope follower light

static const Panel kPanels[] = { { 16, PY, 420, PH, "FILTER" }, { 444, PY, 260, PH, "ENVELOPE FOLLOWER" }, { 712, PY, 172, PH, "GAIN" } };

static std::vector<Ctl> buildControls() {
    std::vector<Ctl> c;
    c.push_back({ Segment, kResponse, 316, PY + 28, 200, "" });
    c.push_back({ Knob, kFreq,    126, PY + 116, 50, "FREQUENCY" });
    c.push_back({ Knob, kPeak,    326, PY + 116, 42, "PEAK" });
    c.push_back({ Knob, kEfLevel, 514, PY + 120, 32, "EF LEVEL" });
    c.push_back({ Knob, kEfSpeed, 634, PY + 120, 32, "EF SPEED" });
    c.push_back({ Knob, kInGain,  756, PY + 116, 26, "INPUT" });
    c.push_back({ Knob, kOutGain, 840, PY + 116, 26, "OUTPUT" });
    return c;
}
static const std::vector<Ctl>& controls() { static std::vector<Ctl> c = buildControls(); return c; }

static const char* const* segLabels(uint32_t p, int& n) {
    static const char* const resp[] = { "LOW PASS", "HIGH PASS" };
    if (p == kResponse) { n = 2; return resp; }
    n = 0; return nullptr;
}

static constexpr float PS_X = 268.f, PS_Y = 20.f, PS_W = 200.f, PS_H = 28.f, PS_ARROW = 28.f;
static constexpr float A_X = 476.f, AB_W = 30.f, COPY_X = 540.f, COPY_W = 48.f;
static constexpr float UNDO_X = 598.f, REDO_X = 650.f, UR_W = 48.f, LEARN_X = 708.f, LEARN_W = 66.f, MIDI_X = 778.f, MIDI_W = 48.f, HELP_X = 832.f, HELP_W = 48.f;
static constexpr float PM_COLW = 150.f, PM_ROW = 23.f, PM_HEAD = 22.f, PM_PAD = 8.f;

class FlintUI : public UI {
public:
    FlintUI() : UI((uint)W, (uint)H), resizeHandle(this) {
        loadSharedResources();
        Engine def;
        for (uint32_t i = 0; i < kParamCount; ++i) { val[i] = def.p[i]; defv[i] = def.p[i]; }
        for (int& m : paramCC) m = -1;
        const double sf = getScaleFactor();
        if (sf != 1.0) setSize((uint)(W * sf), (uint)(H * sf));
        setGeometryConstraints((uint)(W * 0.6 * sf), (uint)(H * 0.6 * sf), true);
        if (isResizable()) resizeHandle.hide();
        userPresets = listUserPresets();
        std::copy(val, val + kParamCount, before);
    }

protected:
    float uiScale() const { return (float)getWidth() / W; }

    // ---- values -------------------------------------------------------------
    void setValue(uint32_t p, float v) {
        const ParamDef& d = paramDef(p);
        v = clampf(v, d.min, d.max);
        if (v == val[p]) return;
        val[p] = v; setParameterValue(p, v); repaint();
    }
    void commit(uint32_t p, float v) { editParameter(p, true); setValue(p, v); editParameter(p, false); }

    void parameterChanged(uint32_t i, float v) override {
        if (i >= kParamCount) return;
        if (i == kOutLearn) {
            if (v != lastLearnEcho && v > 0) {
                const int code = (int)v % 10000 - 1, prm = code / 128, cc = code % 128;
                for (int& m : paramCC) if (m == cc) m = -1;
                if (prm >= 0 && prm < (int)kParamCount) paramCC[prm] = cc;
                learnWaiting = -1;
            }
            lastLearnEcho = v; repaint(); return;
        }
        if (i == kOutEf) { if (std::fabs(v - val[i]) > 0.005f) { val[i] = v; repaint(); } return; }
        if (val[i] != v) { val[i] = v; if (!gesture) before[i] = v; repaint(); }   // host/MIDI changes are not undo steps
    }
    void stateChanged(const char* key, const char* value) override {
        if (std::strcmp(key, "midimap") != 0) return;
        for (int& m : paramCC) m = -1;
        const char* q = value;
        while (q && *q) {
            int cc = -1, prm = -1;
            if (std::sscanf(q, "%d:%d", &cc, &prm) == 2 && prm >= 0 && prm < (int)kParamCount) paramCC[prm] = cc;
            const char* s = std::strchr(q, ';'); if (!s) break; q = s + 1;
        }
        repaint();
    }

    // ---- undo / redo / A-B: whole-state snapshots (last entry = A/B slot) -------------
    typedef std::vector<float> Snap;
    Snap snapshot() const { Snap s(val, val + kParamCount); s.push_back((float)abSlot); return s; }
    bool differs(const float* a, const float* b) const {
        for (uint32_t i = 0; i < kParamCount; ++i) if (!(paramDef(i).hints & kHintOutput) && a[i] != b[i]) return true;
        return false;
    }
    void beginGesture() { gesture = true; std::copy(val, val + kParamCount, before); }
    void endGesture() {
        gesture = false;
        if (differs(before, val)) {
            Snap u(before, before + kParamCount); u.push_back((float)abSlot);
            undoStack.push_back(u); if (undoStack.size() > 200) undoStack.erase(undoStack.begin());
            redoStack.clear();
        }
        std::copy(val, val + kParamCount, before); repaint();
    }
    void applyAll(const float* s) { for (uint32_t i = 0; i < kParamCount; ++i) if (!(paramDef(i).hints & kHintOutput) && val[i] != s[i]) commit(i, s[i]); }
    void restoreSnap(const Snap& s) {
        if (s.size() > kParamCount && (int)s[kParamCount] != abSlot) { abStore[abSlot] = snapshot(); abSlot = (int)s[kParamCount]; }
        applyAll(s.data()); std::copy(val, val + kParamCount, before);
    }
    void undo() { if (undoStack.empty()) return; redoStack.push_back(snapshot()); restoreSnap(undoStack.back()); undoStack.pop_back(); flash("Undo"); }
    void redo() { if (redoStack.empty()) return; undoStack.push_back(snapshot()); restoreSnap(redoStack.back()); redoStack.pop_back(); flash("Redo"); }
    void switchAB(int to) {
        if (to == abSlot) return;
        abStore[abSlot] = snapshot(); if (abStore[to].empty()) abStore[to] = snapshot();
        beginGesture(); applyAll(abStore[to].data()); endGesture();
        abSlot = to; flash(to == 0 ? "Setting A" : "Setting B");
    }
    void copyAB() { abStore[1 - abSlot] = snapshot(); flash(abSlot == 0 ? "Copied A to B" : "Copied B to A"); }

    // ---- formatting ---------------------------------------------------------
    void fmtVal(uint32_t p, float v, char* b, size_t n) const {
        int ne = 0;
        if (const char* const* l = enumLabels(p, ne)) { snprintf(b, n, "%s", l[std::max(0, std::min(ne - 1, (int)std::round(v)))]); return; }
        const ParamDef& d = paramDef(p);
        if (d.hints & kHintBoolean) { snprintf(b, n, "%s", v > 0.5f ? "On" : "Off"); return; }
        if (p == kFreq) { if (v >= 1000) snprintf(b, n, "%.2f kHz", v / 1000); else snprintf(b, n, "%.0f Hz", v); return; }
        if (p == kInGain || p == kOutGain) { snprintf(b, n, "%+.1f dB", v); return; }
        if (p == kEfLevel) { if (std::fabs(v) < 0.005f) snprintf(b, n, "Off"); else snprintf(b, n, "%+.0f%%", v * 100); return; }
        if (p == kEfSpeed) { snprintf(b, n, "%.0f%%", v * 100); return; }
        snprintf(b, n, "%.0f%%", v * 100);
    }
    void fmt(uint32_t p, char* b, size_t n) const { fmtVal(p, val[p], b, n); }
    bool parse(uint32_t p, const std::string& in, float& out) const {
        std::string s; for (char ch : in) if (!std::isspace((unsigned char)ch)) s += (char)std::tolower((unsigned char)ch);
        if (s.empty()) return false;
        char* end = nullptr; double x = std::strtod(s.c_str(), &end);
        if (end == s.c_str()) return false;
        std::string suf(end);
        if (suf == "k" || suf == "khz") x *= 1000;
        if (p == kFreq || p == kInGain || p == kOutGain) { out = (float)x; return true; }
        out = (float)(x / 100.0); return true;
    }

    // ---- drawing ------------------------------------------------------------
    void onNanoDisplay() override {
        const float s = uiScale();
        save(); scale(s, s);
        beginPath(); rect(0, 0, W, H); fillColor(C_BG); fill();
        fontFace(NANOVG_DEJAVU_SANS_TTF);

        // header: a knapped flint shard with a spark
        beginPath(); moveTo(18, 40); lineTo(26, 18); lineTo(38, 14); lineTo(44, 26); lineTo(36, 46); lineTo(24, 48); closePath();
        fillColor(Color(0x55, 0x55, 0x5a)); fill();
        beginPath(); moveTo(26, 18); lineTo(38, 14); lineTo(33, 30); closePath(); fillColor(Color(0x7a, 0x7a, 0x80)); fill();
        beginPath(); circle(44, 16, 3); fillColor(C_ACC); fill();
        beginPath(); moveTo(44, 10); lineTo(44, 22); moveTo(38, 16); lineTo(50, 16); strokeColor(withAlpha(C_ACC, 0.7f)); strokeWidth(1); stroke();
        fontSize(22); textLetterSpacing(9); fillColor(C_TEXT); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); text(58, 34, "FLINT", nullptr);
        fontSize(9.5f); textLetterSpacing(2.5f); fillColor(C_DIM); text(170, 29, "FILTER", nullptr);
        fontSize(7); textLetterSpacing(1.1f); fillColor(C_FAINT); text(170, 41, "EPSILON AUDIO 1.0", nullptr);
        drawPresetBar();
        for (int k = 0; k < 2; ++k) {
            const float x = A_X + k * AB_W; const bool on = abSlot == k, hot = hoverHeader == 6 + k;
            beginPath(); roundedRect(x, PS_Y, AB_W - 2, PS_H, 5); fillColor(on ? withAlpha(C_ACC, 0.25f) : C_PANEL); fill();
            strokeColor(on ? C_ACC : (hot ? C_DIM : C_LINE)); strokeWidth(1); stroke();
            fontSize(10); textLetterSpacing(0); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(on ? C_TEXT : C_DIM);
            text(x + (AB_W - 2) / 2, PS_Y + PS_H / 2 + 0.5f, k == 0 ? "A" : "B", nullptr);
        }
        drawHeaderButton(COPY_X, COPY_W, "COPY", false, hoverHeader == 8, false);
        drawHeaderButton(UNDO_X, UR_W, "UNDO", false, hoverHeader == 3, undoStack.empty());
        drawHeaderButton(REDO_X, UR_W, "REDO", false, hoverHeader == 4, redoStack.empty());
        drawHeaderButton(LEARN_X, LEARN_W, learnMode ? "LEARNING" : "LEARN", learnMode, hoverHeader == 1, false);
        drawHeaderButton(MIDI_X, MIDI_W, "MIDI", val[kMidiOn] > 0.5f, hoverHeader == 2, false);
        drawHeaderButton(HELP_X, HELP_W, "HELP", helpOpen, hoverHeader == 5, false);

        drawDisplay();
        for (const Panel& p : kPanels) drawPanel(p);
        drawLight();

        const std::vector<Ctl>& cs = controls();
        for (int i = 0; i < (int)cs.size(); ++i) drawCtl(cs[i], i == active || i == hover);
        if (editing >= 0) drawEditBox();
        drawStatus();
        if (menuOpen) drawPresetMenu();
        if (saving) drawSaveBox();
        if (helpOpen) drawHelp();
        else if (tipShown && gTipsOn) drawTooltip();
        restore();
    }

    void drawHeaderButton(float x, float w, const char* label, bool on, bool hot, bool disabled) {
        beginPath(); roundedRect(x, PS_Y, w, PS_H, 5); fillColor(on ? withAlpha(C_ACC, 0.22f) : C_PANEL); fill();
        strokeColor(on ? C_ACC : (hot && !disabled ? C_DIM : C_LINE)); strokeWidth(1); stroke();
        fontSize(9); textLetterSpacing(1.6f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(disabled ? C_FAINT : (on ? C_TEXT : C_DIM));
        text(x + w / 2, PS_Y + PS_H / 2 + 0.5f, label, nullptr);
    }
    void drawPanel(const Panel& p) {
        beginPath(); roundedRect(p.x, p.y, p.w, p.h, 6); fillColor(C_PANEL); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
        fontSize(8.5f); textLetterSpacing(2.2f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(C_FAINT); text(p.x + 20, p.y + 28, p.title, nullptr);
    }
    void drawLight() {
        const float m = val[kOutEf], a = clampf(std::sqrt(std::fabs(m) / kEfOctaves) * 1.25f, 0.f, 1.f);
        const Color col = m >= 0 ? C_ACC : C_EF;
        const bool live = std::fabs(val[kEfLevel]) > 0.005f;
        fontSize(8.5f); textLetterSpacing(2.f); textAlign(ALIGN_RIGHT | ALIGN_MIDDLE); fillColor(live ? C_DIM : C_FAINT); text(LIGHT_X - 14, LIGHT_Y, "EF", nullptr);
        if (a > 0.02f) { beginPath(); circle(LIGHT_X, LIGHT_Y, 12); fillPaint(radialGradient(LIGHT_X, LIGHT_Y, 2, 12, withAlpha(col, 0.5f * a), withAlpha(col, 0))); fill(); }
        beginPath(); circle(LIGHT_X, LIGHT_Y, 5); fillColor(a > 0.02f ? withAlpha(col, 0.35f + 0.65f * a) : C_PANEL_HI); fill();
        strokeColor(live ? C_LINE : withAlpha(C_LINE, 0.6f)); strokeWidth(1); stroke();
    }

    // display mapping
    static float xOfHz(float hz) { return DX + 14 + (DW - 28) * std::log(hz / 20.f) / std::log(1000.f); }
    static float hzOfX(float x)  { return 20.f * std::pow(1000.f, (x - DX - 14) / (DW - 28)); }
    static float yOfDb(float db) { return DY + 12 + (DH - 34) * (1 - (db + 30.f) / 54.f); }
    static float peakOfY(float y) { return (DY + DH * 0.84f - y) / (DH * 0.66f); }

    void curvePath(float extraOct, std::vector<float>& ys, int N) const {
        ys.resize(N + 1);
        for (int i = 0; i <= N; ++i) {
            const float hz = hzOfX(DX + 14 + (DW - 28) * i / N);
            ys[i] = clampf(yOfDb(20 * std::log10(std::max(Engine::magnitude(val, hz, extraOct), 1e-5f))), DY - 40, DY + DH + 40);
        }
    }
    void drawDisplay() {
        beginPath(); roundedRect(DX, DY, DW, DH, 6); fillColor(C_DISPLAY); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
        fontSize(8.5f); textLetterSpacing(1); fillColor(C_FAINT); textAlign(ALIGN_CENTER | ALIGN_BOTTOM);
        const float grid[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
        const char* gl[] = { "50", "100", "200", "500", "1k", "2k", "5k", "10k" };
        beginPath();
        for (float g : grid) { const float x = xOfHz(g); moveTo(x, DY + 6); lineTo(x, DY + DH - 18); }
        for (int db = -24; db <= 12; db += 12) { const float y = yOfDb((float)db); moveTo(DX + 6, y); lineTo(DX + DW - 6, y); }
        strokeColor(Color(255, 255, 255, 9 / 255.f)); strokeWidth(1); stroke();
        for (int i = 0; i < 8; ++i) text(xOfHz(grid[i]), DY + DH - 4, gl[i], nullptr);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        for (int db = -24; db <= 12; db += 12) { char b[8]; snprintf(b, sizeof b, "%+d", db); text(DX + 8, yOfDb((float)db) - 7, db == 0 ? "0 dB" : b, nullptr); }

        const int N = 300;
        std::vector<float> ys;
        scissor(DX + 1, DY + 1, DW - 2, DH - 19);     // curves run off the edge instead of flattening along it
        // where the envelope follower has the filter right now
        const float m = val[kOutEf];
        if (std::fabs(val[kEfLevel]) > 0.005f && std::fabs(m) > 0.02f) {
            curvePath(m, ys, N);
            beginPath();
            for (int i = 0; i <= N; ++i) { const float x = DX + 14 + (DW - 28) * i / N; if (!i) moveTo(x, ys[i]); else lineTo(x, ys[i]); }
            strokeColor(withAlpha(m >= 0 ? C_ACC : C_EF, 0.45f)); strokeWidth(1.4f); stroke();
        }
        curvePath(0, ys, N);
        beginPath(); moveTo(DX + 14, DY + DH + 40);
        for (int i = 0; i <= N; ++i) lineTo(DX + 14 + (DW - 28) * i / N, ys[i]);
        lineTo(DX + DW - 14, DY + DH + 40); closePath();
        fillPaint(linearGradient(0, DY, 0, DY + DH, withAlpha(C_ACC, 0.2f), withAlpha(C_ACC, 0.f))); fill();
        beginPath();
        for (int i = 0; i <= N; ++i) { const float x = DX + 14 + (DW - 28) * i / N; if (!i) moveTo(x, ys[i]); else lineTo(x, ys[i]); }
        strokeColor(withAlpha(C_ACC, 0.95f)); strokeWidth(2.2f); stroke();
        resetScissor();

        // handle, with the envelope follower's reach drawn as a bar
        float hx, hy; handlePos(hx, hy);
        const float lv = val[kEfLevel];
        if (std::fabs(lv) > 0.005f) {
            const float x2 = clampf(xOfHz(val[kFreq] * std::exp2(lv * kEfOctaves)), DX + 10, DX + DW - 10);
            const Color col = lv > 0 ? C_ACC : C_EF;
            beginPath(); moveTo(hx, hy); lineTo(x2, hy); strokeColor(withAlpha(col, 0.35f)); strokeWidth(2); stroke();
            beginPath(); moveTo(x2, hy - 6); lineTo(x2, hy + 6); strokeColor(withAlpha(col, 0.6f)); strokeWidth(1.5f); stroke();
            if (std::fabs(m) > 0.02f) { const float xm = clampf(xOfHz(val[kFreq] * std::exp2(m)), DX + 10, DX + DW - 10);
                beginPath(); circle(xm, hy, 4); fillColor(withAlpha(col, 0.9f)); fill(); }
        }
        const bool hot = dragHandle || hoverHandle;
        if (hot) { beginPath(); circle(hx, hy, 20); fillColor(withAlpha(C_ACC, 0.15f)); fill(); }
        beginPath(); circle(hx, hy, hot ? 13 : 11); fillColor(C_ACC); fill();
        beginPath(); circle(hx, hy, 4); fillColor(C_BG); fill();
    }
    // the dot sits on the curve at the cutoff; dragging it up and down changes the peak
    void handlePos(float& x, float& y) const {
        x = xOfHz(val[kFreq]);
        y = clampf(yOfDb(20 * std::log10(Engine::magnitude(val, val[kFreq]))), DY + 16, DY + DH - 26);
    }

    void drawKnobBody(float x, float y, float r, float n, bool bipolar, bool hot, Color col) {
        const float a0 = 0.75f * 3.14159265f, a1 = 2.25f * 3.14159265f, ang = a0 + (a1 - a0) * n;
        const float tw = r > 30 ? 6.f : (r > 18 ? 4.f : 3.f);
        beginPath(); arc(x, y, r, a0, a1, CW); strokeColor(C_LINE); strokeWidth(tw); lineCap(ROUND); stroke();
        if (bipolar) {
            const float mid = (a0 + a1) / 2;
            if (std::fabs(ang - mid) > 0.01f) { beginPath(); arc(x, y, r, std::min(mid, ang), std::max(mid, ang), CW); strokeColor(col); stroke(); }
            beginPath(); moveTo(x, y - r - tw / 2 - 2); lineTo(x, y - r - tw / 2 - 6); strokeColor(C_FAINT); strokeWidth(1.5f); stroke();
        } else if (ang - a0 > 0.001f) { beginPath(); arc(x, y, r, a0, ang, CW); strokeColor(col); stroke(); }
        const float cr = r - (r > 30 ? 12 : (r > 18 ? 8 : 6));
        beginPath(); circle(x, y, cr);
        fillPaint(radialGradient(x - cr * 0.3f, y - cr * 0.4f, 0, cr * 1.4f, hot ? Color(0x3c, 0x3c, 0x40) : C_PANEL_HI, Color(0x13, 0x13, 0x14)));
        fill(); strokeColor(Color(255, 255, 255, 18 / 255.f)); strokeWidth(1); stroke();
        beginPath(); moveTo(x + std::cos(ang) * cr * 0.35f, y + std::sin(ang) * cr * 0.35f); lineTo(x + std::cos(ang) * cr * 0.9f, y + std::sin(ang) * cr * 0.9f);
        strokeColor(C_TEXT); strokeWidth(r > 30 ? 3.f : 2.f); lineCap(ROUND); stroke();
    }

    void drawCtl(const Ctl& c, bool hot) {
        const uint32_t p = c.param;
        const Color col = (p == kEfLevel && val[p] < 0) ? C_EF : C_ACC;
        char buf[40];
        switch (c.type) {
        case Knob: {
            drawKnobBody(c.x, c.y, c.r, toNorm(p, val[p]), (paramDef(p).hints & kHintBipolar) != 0, hot, col);
            const bool big = c.r > 30;
            fontSize(big ? 9.5f : 8.5f); textLetterSpacing(big ? 2.f : 1.6f); textAlign(ALIGN_CENTER | ALIGN_TOP);
            if (big) {            // the big knobs always show their value under the name
                fillColor(C_DIM); text(c.x, c.y + c.r + 6, c.label, nullptr);
                fmt(p, buf, sizeof buf); fontSize(12); textLetterSpacing(0.6f); fillColor(hot ? col : C_TEXT); text(c.x, c.y + c.r + 20, buf, nullptr);
            } else {
                fillColor(C_DIM); text(c.x, c.y + c.r + 6, c.label, nullptr);
                fmt(p, buf, sizeof buf); fontSize(11); textLetterSpacing(0.4f); fillColor(hot ? col : Color(0xc8, 0xc6, 0xc2)); text(c.x, c.y + c.r + 20, buf, nullptr);
            }
            break; }
        case Segment: {
            int n = 0; const char* const* names = segLabels(p, n);
            const int cur = (int)(val[p] + .5f); const float sw = c.r / n, x0 = c.x - c.r / 2;
            for (int i = 0; i < n; ++i) {
                const float x = x0 + sw * i;
                beginPath(); roundedRect(x + 1.5f, c.y - SEG_H / 2, sw - 3, SEG_H, 5); fillColor(i == cur ? C_ACC : C_PANEL_HI); fill();
                fontSize(9.5f); textLetterSpacing(1.6f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
                fillColor(i == cur ? C_BG : C_DIM); text(x + sw / 2, c.y + 0.5f, names[i], nullptr);
            }
            break; }
        }
        if (learnMode) {
            const int cc = paramCC[p]; const bool waiting = learnWaiting == (int)p;
            if (cc >= 0 || waiting) {
                const float by = c.type == Knob ? c.y - c.r - 14 : c.y - SEG_H / 2 - 8;
                snprintf(buf, sizeof buf, waiting ? "?" : "CC%d", cc);
                fontSize(8.5f); textLetterSpacing(0.5f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
                const float bw = waiting ? 16 : 34;
                beginPath(); roundedRect(c.x - bw / 2, by - 7, bw, 14, 3); fillColor(waiting ? C_ACC : C_PANEL_HI); fill(); strokeColor(C_ACC); strokeWidth(1); stroke();
                fillColor(waiting ? C_BG : C_ACC); text(c.x, by + 0.5f, buf, nullptr);
            }
        }
    }

    void drawStatus() {
        std::string msg; char vb[40];
        if (statusFlash > 0) msg = statusText;
        else if (learnMode) msg = learnWaiting >= 0 ? std::string("Move a MIDI controller to assign: ") + paramDef(learnWaiting).name
                                                  : "MIDI Learn: click a control, then move a controller.  Right-click a control to unassign.";
        else if (hover >= 0) { const uint32_t p = controls()[hover].param; fmt(p, vb, sizeof vb); msg = std::string(paramDef(p).name) + "   " + vb; }
        else if (hoverHandle || dragHandle) {
            char a[24], b[24]; fmt(kFreq, a, sizeof a); fmt(kPeak, b, sizeof b);
            msg = std::string(a) + "   peak " + b + "      drag: left/right = frequency, up/down = peak";
        }
        if (!msg.empty()) { fontSize(10); textLetterSpacing(0.8f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(learnMode || statusFlash > 0 ? C_ACC : C_DIM); text(DX + 48, DY + 16, msg.c_str(), nullptr); }
        if (learnMode && learnWaiting < 0) { textAlign(ALIGN_RIGHT | ALIGN_MIDDLE); fillColor(hoverClear ? C_TEXT : C_DIM); text(DX + DW - 14, DY + 16, "CLEAR ALL ASSIGNMENTS", nullptr); }
        if (statusFlash > 0) { --statusFlash; repaint(); }
    }
    void flash(const std::string& s) { statusText = s; statusFlash = 60; repaint(); }

    void drawEditBox() {
        const Ctl& c = controls()[editing];
        const float w = 90, h = 22, x = c.x - w / 2, y = c.y + c.r + 16;
        beginPath(); roundedRect(x, y, w, h, 4); fillColor(C_DISPLAY); fill(); strokeColor(C_ACC); strokeWidth(1); stroke();
        fontSize(12); textLetterSpacing(0.5f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_TEXT); text(c.x, y + h / 2 + 0.5f, (editBuf + "|").c_str(), nullptr);
    }

    // ---- presets ------------------------------------------------------------------
    bool matchesValues(const float* v) const {
        for (uint32_t i = 0; i < kParamCount; ++i) {
            if (paramDef(i).hints & kHintOutput) continue;
            if (std::fabs(val[i] - v[i]) > 1e-4f * std::max(1.f, std::fabs(v[i]))) return false;
        }
        return true;
    }
    void applyValues(const float* v) { beginGesture(); applyAll(v); endGesture(); }
    void applyFactory(int idx) { idx = (idx % (int)kNumPresets + (int)kNumPresets) % (int)kNumPresets; applyValues(presets()[idx].v); curFactory = idx; curUser.clear(); repaint(); }
    void applyUser(const std::string& name) { float v[kParamCount]; if (!loadUserPreset(name, v)) return; applyValues(v); std::copy(v, v + kParamCount, userVals); curUser = name; curFactory = -1; repaint(); }
    void programLoaded(uint32_t index) override { curFactory = (int)index; curUser.clear(); repaint(); }
    void step(int d) {
        const int nf = (int)kNumPresets, nu = (int)userPresets.size(), total = nf + nu;
        int cur = curFactory; if (!curUser.empty()) for (int i = 0; i < nu; ++i) if (userPresets[i] == curUser) cur = nf + i;
        if (cur < 0) cur = 0;
        const int next = ((cur + d) % total + total) % total;
        if (next < nf) applyFactory(next); else applyUser(userPresets[next - nf]);
    }
    void drawPresetBar() {
        const float x = PS_X, y = PS_Y, w = PS_W, h = PS_H, a = PS_ARROW;
        beginPath(); roundedRect(x, y, w, h, 5); fillColor(C_PANEL); fill(); strokeColor(menuOpen ? C_ACC : C_LINE); strokeWidth(1); stroke();
        beginPath(); moveTo(x + a, y + 5); lineTo(x + a, y + h - 5); moveTo(x + w - a, y + 5); lineTo(x + w - a, y + h - 5); strokeColor(C_LINE); stroke();
        for (int side = 0; side < 2; ++side) {
            const bool hot = presetHover == (side == 0 ? 0 : 2);
            const float cx = side == 0 ? x + a / 2 : x + w - a / 2, cy = y + h / 2, d = side == 0 ? -1.f : 1.f;
            beginPath(); moveTo(cx - 3 * d, cy - 5); lineTo(cx + 3 * d, cy); lineTo(cx - 3 * d, cy + 5); closePath(); fillColor(hot ? C_ACC : C_DIM); fill();
        }
        std::string name; bool modified = false;
        if (!curUser.empty()) { name = curUser; modified = !matchesValues(userVals); }
        else if (curFactory >= 0) { name = presets()[curFactory].name; modified = !matchesValues(presets()[curFactory].v); }
        else name = "Custom";
        if (modified) name += " *";
        fontSize(11.5f); textLetterSpacing(0.6f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(presetHover == 1 || menuOpen ? C_TEXT : Color(0xd2, 0xd0, 0xcc));
        text(x + w / 2, y + h / 2 + 0.5f, name.c_str(), nullptr);
    }
    struct MRect { float x, y, w, h; int kind; int idx; };
    void menuLayout(float& mx, float& my, float& mw, float& mh, std::vector<MRect>& items) const {
        const int cols = kNumCategories + 1;
        mw = PM_COLW * cols + 2 * PM_PAD; mx = std::min(W - mw - 8, std::max(8.f, PS_X + PS_W / 2 - mw / 2)); my = PS_Y + PS_H + 4;
        items.clear();
        const float top = my + PM_PAD + PM_ROW + 6 + PM_HEAD;
        int rows[kNumCategories + 1] = {}; int maxRows = 1;
        for (uint32_t i = 0; i < kNumPresets; ++i) {
            const char* cat = presets()[i].category;
            if (std::strcmp(cat, "Init") == 0) { items.push_back({ mx + PM_PAD, my + PM_PAD, PM_COLW - 4, PM_ROW, 0, (int)i }); continue; }
            for (int c = 0; c < kNumCategories; ++c)
                if (std::strcmp(cat, kCategories[c]) == 0) { items.push_back({ mx + PM_PAD + c * PM_COLW, top + rows[c] * PM_ROW, PM_COLW - 4, PM_ROW, 0, (int)i }); maxRows = std::max(maxRows, ++rows[c]); }
        }
        for (int u = 0; u < (int)userPresets.size() && u < 16; ++u) items.push_back({ mx + PM_PAD + kNumCategories * PM_COLW, top + u * PM_ROW, PM_COLW - 4, PM_ROW, 1, u });
        maxRows = std::max(maxRows, std::min(16, (int)userPresets.size()));
        items.push_back({ mx + mw - PM_PAD - 150, my + PM_PAD, 150, PM_ROW, 2, 0 });
        mh = PM_PAD + PM_ROW + 6 + PM_HEAD + maxRows * PM_ROW + PM_PAD;
    }
    void drawPresetMenu() {
        float mx, my, mw, mh; std::vector<MRect> items; menuLayout(mx, my, mw, mh, items);
        beginPath(); rect(mx - 12, my - 4, mw + 24, mh + 20); fillPaint(boxGradient(mx, my + 2, mw, mh, 6, 14, Color(0, 0, 0, 0.6f), Color(0, 0, 0, 0.f))); fill();
        beginPath(); roundedRect(mx, my, mw, mh, 6); fillColor(C_PANEL_HI); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
        beginPath(); moveTo(mx + PM_PAD, my + PM_PAD + PM_ROW + 3); lineTo(mx + mw - PM_PAD, my + PM_PAD + PM_ROW + 3); strokeColor(C_LINE); stroke();
        fontSize(8.5f); textLetterSpacing(2.f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        for (int c = 0; c <= kNumCategories; ++c) {
            std::string up = c < kNumCategories ? kCategories[c] : "User"; for (char& ch : up) ch = (char)std::toupper((unsigned char)ch);
            fillColor(c < kNumCategories ? C_ACC : C_EF); text(mx + PM_PAD + c * PM_COLW + 12, my + PM_PAD + PM_ROW + 6 + PM_HEAD / 2, up.c_str(), nullptr);
        }
        if (userPresets.empty()) { fontSize(10); textLetterSpacing(0.4f); fillColor(C_FAINT); text(mx + PM_PAD + kNumCategories * PM_COLW + 12, my + PM_PAD + PM_ROW + 6 + PM_HEAD + PM_ROW / 2, "(none saved yet)", nullptr); }
        for (int i = 0; i < (int)items.size(); ++i) {
            const MRect& r = items[i]; const bool hot = i == menuHover;
            if (r.kind == 2) {
                beginPath(); roundedRect(r.x, r.y + 1, r.w, r.h - 2, 4); fillColor(hot ? withAlpha(C_EF, 0.25f) : C_PANEL); fill(); strokeColor(C_EF); strokeWidth(1); stroke();
                fontSize(9.5f); textLetterSpacing(1.6f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_TEXT); text(r.x + r.w / 2, r.y + r.h / 2 + 0.5f, "SAVE CURRENT AS...", nullptr);
                continue;
            }
            const bool isCur = r.kind == 0 ? (curUser.empty() && r.idx == curFactory) : (curUser == userPresets[r.idx]);
            const bool del = r.kind == 1 && pendingDelete == r.idx;
            if (hot || del) { beginPath(); roundedRect(r.x, r.y, r.w, r.h, 4); fillColor(del ? Color(0xd0, 0x50, 0x40, 0.35f) : withAlpha(C_ACC, 0.18f)); fill(); }
            if (isCur) { beginPath(); circle(r.x + 6, r.y + r.h / 2, 2.5f); fillColor(r.kind == 0 ? C_ACC : C_EF); fill(); }
            fontSize(11); textLetterSpacing(0.5f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(hot || isCur || del ? C_TEXT : C_DIM);
            const std::string label = del ? "Delete? click again" : (r.kind == 0 ? std::string(presets()[r.idx].name) : userPresets[r.idx]);
            text(r.x + 12, r.y + r.h / 2 + 0.5f, label.c_str(), nullptr);
        }
    }
    int menuHit(float x, float y) const { float mx, my, mw, mh; std::vector<MRect> it; menuLayout(mx, my, mw, mh, it);
        for (int i = 0; i < (int)it.size(); ++i) if (x >= it[i].x && x < it[i].x + it[i].w && y >= it[i].y && y < it[i].y + it[i].h) return i;
        return -1; }
    MRect menuItem(int i) const { float mx, my, mw, mh; std::vector<MRect> it; menuLayout(mx, my, mw, mh, it); return it[i]; }
    void drawSaveBox() {
        const float w = 360, h = 96, x = W / 2 - w / 2, y = 130;
        beginPath(); rect(0, 0, W, H); fillColor(Color(0, 0, 0, 0.45f)); fill();
        beginPath(); roundedRect(x, y, w, h, 8); fillColor(C_PANEL_HI); fill(); strokeColor(C_EF); strokeWidth(1); stroke();
        fontSize(9.5f); textLetterSpacing(2.f); textAlign(ALIGN_LEFT | ALIGN_TOP); fillColor(C_DIM); text(x + 16, y + 14, "SAVE PRESET AS", nullptr);
        beginPath(); roundedRect(x + 16, y + 36, w - 32, 26, 4); fillColor(C_DISPLAY); fill(); strokeColor(C_LINE); stroke();
        fontSize(13); textLetterSpacing(0.4f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(C_TEXT); text(x + 26, y + 49.5f, (saveName + "|").c_str(), nullptr);
        fontSize(8.5f); textLetterSpacing(1.2f); fillColor(C_FAINT); textAlign(ALIGN_RIGHT | ALIGN_TOP);
        text(x + w - 16, y + 72, saveError ? "COULD NOT SAVE - CHECK THE NAME" : "ENTER TO SAVE   ·   ESC TO CANCEL", nullptr);
    }

    // ---- interaction --------------------------------------------------------------
    int hit(float x, float y, int* seg = nullptr) const {
        const std::vector<Ctl>& cs = controls();
        for (int i = 0; i < (int)cs.size(); ++i) {
            const Ctl& c = cs[i];
            switch (c.type) {
            case Knob: { const float rr = c.r + 6; if ((x - c.x) * (x - c.x) + (y - c.y) * (y - c.y) <= rr * rr) return i; break; }
            case Segment: { int n = 0; segLabels(c.param, n); const float x0 = c.x - c.r / 2;
                if (x >= x0 && x < x0 + c.r && std::fabs(y - c.y) <= SEG_H / 2 + 1) { if (seg) *seg = (int)((x - x0) / (c.r / n)); return i; } break; }
            }
        }
        return -1;
    }
    bool inDisplay(float x, float y) const { return x > DX && x < DX + DW && y > DY && y < DY + DH; }
    bool handleHit(float x, float y) const { float hx, hy; handlePos(hx, hy); return (x - hx) * (x - hx) + (y - hy) * (y - hy) <= 18 * 18; }
    bool lightHit(float x, float y) const { return std::fabs(x - LIGHT_X + 6) <= 18 && std::fabs(y - LIGHT_Y) <= 10; }
    int headerHit(float x, float y) const {
        if (y < PS_Y || y > PS_Y + PS_H) return 0;
        if (x >= LEARN_X && x <= LEARN_X + LEARN_W) return 1;
        if (x >= MIDI_X && x <= MIDI_X + MIDI_W) return 2;
        if (x >= UNDO_X && x <= UNDO_X + UR_W) return 3;
        if (x >= REDO_X && x <= REDO_X + UR_W) return 4;
        if (x >= HELP_X && x <= HELP_X + HELP_W) return 5;
        if (x >= A_X && x < A_X + AB_W) return 6;
        if (x >= A_X + AB_W && x < A_X + 2 * AB_W) return 7;
        if (x >= COPY_X && x <= COPY_X + COPY_W) return 8;
        return 0;
    }
    int presetBarHit(float x, float y) const {
        if (y < PS_Y || y > PS_Y + PS_H || x < PS_X || x > PS_X + PS_W) return -1;
        return x < PS_X + PS_ARROW ? 0 : (x > PS_X + PS_W - PS_ARROW ? 2 : 1);
    }
    bool clearHit(float x, float y) const { return learnMode && learnWaiting < 0 && y > DY + 8 && y < DY + 24 && x > DX + DW - 190 && x < DX + DW - 10; }

    void setLearn(bool on) { learnMode = on; learnWaiting = -1; if (!on) setState("cmd", "cancel"); repaint(); }
    void endEdit(bool apply) {
        if (editing < 0) return;
        const uint32_t p = controls()[editing].param; float v;
        if (apply && parse(p, editBuf, v)) { beginGesture(); commit(p, v); endGesture(); }
        editing = -1; editBuf.clear(); repaint();
    }
    static float angleNorm(const Ctl& c, float x, float y) {
        float a = std::atan2(y - c.y, x - c.x);
        const float a0 = 0.75f * 3.14159265f;
        float rel = a - a0; while (rel < 0) rel += 2 * 3.14159265f;
        if (rel > 1.5f * 3.14159265f) rel = rel > 1.75f * 3.14159265f ? 0.f : 1.5f * 3.14159265f;
        return rel / (1.5f * 3.14159265f);
    }
    void startHandleDrag(float x, float y) {
        beginGesture(); dragHandle = true; dragX0 = x; dragY0 = y; startOct = std::log2(val[kFreq]); startPeak = val[kPeak];
        editParameter(kFreq, true); editParameter(kPeak, true);
    }

    bool onKeyboard(const KeyboardEvent& ev) override {
        if (!ev.press) return false;
        std::string* buf = saving ? &saveName : (editing >= 0 ? &editBuf : nullptr);
        if (!buf) {
            const bool cmd = (ev.mod & (kModifierControl | kModifierSuper)) != 0;
            if (cmd && (ev.key == 'z' || ev.key == 'Z')) { if (ev.mod & kModifierShift) redo(); else undo(); return true; }
            if (cmd && (ev.key == 'y' || ev.key == 'Y')) { redo(); return true; }
            if (ev.key == kKeyEscape && helpOpen) { helpOpen = false; repaint(); return true; }
            if (ev.key == kKeyEscape && learnMode) { setLearn(false); return true; }
            return false;
        }
        if (ev.key == kKeyEnter) {
            if (saving) {
                if (saveUserPreset(saveName, val)) { userPresets = listUserPresets(); curUser = sanitizeName(saveName); curFactory = -1; std::copy(val, val + kParamCount, userVals); saving = false; saveError = false; }
                else saveError = true;
            } else endEdit(true);
            repaint(); return true;
        }
        if (ev.key == kKeyEscape) { if (saving) saving = false; else endEdit(false); repaint(); return true; }
        if (ev.key == kKeyBackspace || ev.key == kKeyDelete) { if (!buf->empty()) buf->pop_back(); repaint(); return true; }
        if (ev.key >= 32 && ev.key < 127 && buf->size() < (saving ? 40u : 12u)) { *buf += (char)ev.key; repaint(); return true; }
        return true;
    }

    bool onMouse(const MouseEvent& ev) override {
        const float s = uiScale(), x = ev.pos.getX() / s, y = ev.pos.getY() / s;
        if (saving) return true;
        if (ev.press) { statusFlash = 0; tipShown = false; tipSuppressed = tipKey; }
        if (helpOpen) { if (ev.press && ev.button == 1) helpClick(x, y); return true; }
        if (ev.button == 3 && ev.press) {
            if (menuOpen) { const int m = menuHit(x, y); if (m >= 0 && menuItem(m).kind == 1) { pendingDelete = menuItem(m).idx; repaint(); } return true; }
            if (learnMode) { const int i = hit(x, y); if (i >= 0) { const uint32_t p = controls()[i].param; char b[24]; snprintf(b, sizeof b, "unmap:%u", p); setState("cmd", b); paramCC[p] = -1; repaint(); } return true; }
            return false;
        }
        if (ev.button != 1) return false;
        if (!ev.press) {
            if (active >= 0) { editParameter(controls()[active].param, false); active = -1; endGesture(); return true; }
            if (dragHandle) { editParameter(kFreq, false); editParameter(kPeak, false); dragHandle = false; endGesture(); return true; }
            return false;
        }
        if (editing >= 0) endEdit(true);
        if (menuOpen) {
            const int m = menuHit(x, y);
            if (m >= 0) {
                const MRect r = menuItem(m);
                if (r.kind == 2) { menuOpen = false; saving = true; saveError = false; saveName = curUser; }
                else if (r.kind == 1 && pendingDelete == r.idx) { deleteUserPreset(userPresets[r.idx]); if (curUser == userPresets[r.idx]) curUser.clear(); userPresets = listUserPresets(); pendingDelete = -1; repaint(); return true; }
                else { menuOpen = false; if (r.kind == 0) applyFactory(r.idx); else applyUser(userPresets[r.idx]); }
            } else menuOpen = false;
            pendingDelete = -1; menuHover = -1; repaint(); return true;
        }
        const int hh = headerHit(x, y);
        if (hh == 1) { setLearn(!learnMode); return true; }
        if (hh == 2) { beginGesture(); commit(kMidiOn, val[kMidiOn] > 0.5f ? 0.f : 1.f); endGesture(); return true; }
        if (hh == 3) { undo(); return true; }
        if (hh == 4) { redo(); return true; }
        if (hh == 5) { helpOpen = true; repaint(); return true; }
        if (hh == 6 || hh == 7) { switchAB(hh - 6); return true; }
        if (hh == 8) { copyAB(); return true; }
        const int pb = presetBarHit(x, y);
        if (pb == 0) { step(-1); return true; }
        if (pb == 2) { step(1); return true; }
        if (pb == 1) { userPresets = listUserPresets(); menuOpen = true; menuHover = -1; pendingDelete = -1; repaint(); return true; }
        if (clearHit(x, y)) { setState("cmd", "clear"); for (int& m : paramCC) m = -1; repaint(); return true; }

        int seg = 0; const int i = hit(x, y, &seg);
        if (i >= 0) {
            const Ctl& c = controls()[i];
            const uint32_t p = c.param;
            if (learnMode) { learnWaiting = (int)p; char b[24]; snprintf(b, sizeof b, "learn:%u", p); setState("cmd", b); repaint(); return true; }
            if (c.type == Segment) { beginGesture(); commit(p, (float)seg); endGesture(); return true; }
            if (ev.mod & (kModifierControl | kModifierSuper)) { beginGesture(); commit(p, defv[p]); endGesture(); return true; }
            if (i == lastClick && ev.time - lastClickTime < 350) {
                lastClick = -1; editing = i;
                char b[40]; fmt(p, b, sizeof b); editBuf = b;
                const size_t sp = editBuf.find(' '); if (sp != std::string::npos) editBuf.resize(sp);
                if (!editBuf.empty() && editBuf.back() == '%') editBuf.pop_back();
                if (editBuf == "Off") editBuf = "0";
                if (p == kFreq && val[p] >= 1000) { char kb[16]; snprintf(kb, sizeof kb, "%.2fk", val[p] / 1000); editBuf = kb; }
                repaint(); return true;
            }
            lastClick = i; lastClickTime = ev.time;
            beginGesture();
            active = i; dragY = y; dragNorm = toNorm(p, val[p]);
            const float dist = std::sqrt((x - c.x) * (x - c.x) + (y - c.y) * (y - c.y));
            circular = dist > c.r * 0.62f;
            editParameter(p, true);
            if (circular) setValue(p, fromNorm(p, angleNorm(c, x, y)));
            repaint(); return true;
        }
        if (!learnMode && inDisplay(x, y)) {
            // grab the dot, or click anywhere in the display to move it there
            if (!handleHit(x, y)) { beginGesture(); commit(kFreq, clampf(hzOfX(x), 20.f, 20000.f)); endGesture(); }
            startHandleDrag(x, y);
            return true;
        }
        return false;
    }

    bool onMotion(const MotionEvent& ev) override {
        const float s = uiScale(), x = ev.pos.getX() / s, y = ev.pos.getY() / s;
        if (saving) return true;
        if (helpOpen) { const int h = helpHit(x, y); if (h != helpHover) { helpHover = h; repaint(); } return true; }
        if (active >= 0) {
            const Ctl& c = controls()[active]; const uint32_t p = c.param;
            if (circular) { setValue(p, fromNorm(p, angleNorm(c, x, y))); return true; }
            const float sens = (ev.mod & kModifierShift) ? 1500.f : 240.f;
            dragNorm = clampf(dragNorm + (dragY - y) / sens, 0.f, 1.f); dragY = y;
            setValue(p, fromNorm(p, dragNorm));
            return true;
        }
        if (dragHandle) {
            const float fine = (ev.mod & kModifierShift) ? 0.2f : 1.f;
            const float dOct = std::log2(hzOfX(dragX0 + (x - dragX0) * fine) / hzOfX(dragX0)), dPk = (peakOfY(dragY0 + (y - dragY0) * fine) - peakOfY(dragY0));
            setValue(kFreq, clampf(std::exp2(startOct + dOct), 20.f, 20000.f));
            setValue(kPeak, clampf(startPeak + dPk, 0.f, 1.f));
            return true;
        }
        if (menuOpen) { const int m = menuHit(x, y); if (m != menuHover) { menuHover = m; repaint(); } return true; }
        const int hh = headerHit(x, y); if (hh != hoverHeader) { hoverHeader = hh; repaint(); }
        const int pb = presetBarHit(x, y); if (pb != presetHover) { presetHover = pb; repaint(); }
        const bool hc = clearHit(x, y); if (hc != hoverClear) { hoverClear = hc; repaint(); }
        const int h = hit(x, y); if (h != hover) { hover = h; repaint(); }
        const bool hd = h < 0 && !learnMode && handleHit(x, y); if (hd != hoverHandle) { hoverHandle = hd; repaint(); }
        const bool lt = lightHit(x, y);
        // hover tips: shown after the pointer rests on one thing for a moment
        const int key = h >= 0 ? h : (hh > 0 ? 1000 + hh : (pb >= 0 ? 2000 + pb : (hd ? 3000 : (lt ? 4000 : -1))));
        tipX = x; tipY = y;
        if (key != tipKey) { const bool was = tipShown; tipKey = key; tipSince = nowSec(); tipShown = false; if (key != tipSuppressed) tipSuppressed = -2; if (was) repaint(); }
        return false;
    }

    bool onScroll(const ScrollEvent& ev) override {
        const float s = uiScale(), sx = ev.pos.getX() / s, sy = ev.pos.getY() / s;
        if (menuOpen || saving || helpOpen) return true;
        if (presetBarHit(sx, sy) >= 0) { step(ev.delta.getY() > 0 ? -1 : 1); return true; }
        const float st = (ev.mod & kModifierShift) ? 0.004f : 0.02f, d = ev.delta.getY() > 0 ? st : -st;
        const int i = hit(sx, sy);
        if (i < 0 || controls()[i].type != Knob) {
            if (inDisplay(sx, sy)) { beginGesture(); commit(kPeak, clampf(val[kPeak] + d, 0.f, 1.f)); endGesture(); return true; }
            return false;
        }
        const uint32_t p = controls()[i].param;
        beginGesture(); commit(p, fromNorm(p, toNorm(p, val[p]) + d)); endGesture();
        return true;
    }

    void uiIdle() override {
        if (gTipsOn && !tipShown && tipKey >= 0 && tipKey != tipSuppressed && active < 0 && !dragHandle && !menuOpen && !saving && !helpOpen && editing < 0
            && nowSec() - tipSince > 0.6) { tipShown = true; repaint(); }
    }

    // ---- hover tips ---------------------------------------------------------------
    bool tipContent(std::string& title, std::string& value, std::string& body) const {
        if (tipKey < 0) return false;
        if (tipKey < 1000) {
            const uint32_t p = controls()[tipKey].param; char b[40]; fmt(p, b, sizeof b);
            title = paramDef(p).name; value = b; body = paramTip(p);
            if (controls()[tipKey].type == Knob) body += "\n\nDrag the middle up/down or grab the rim and turn. Double-click to type a value, Cmd/Ctrl-click to reset.";
            if (paramCC[p] >= 0) { char cc[32]; snprintf(cc, sizeof cc, "   ·   CC%d", paramCC[p]); value += cc; }
            return true;
        }
        int t = -1;
        if (tipKey >= 4000) t = kTipEfLight;
        else if (tipKey >= 3000) t = kTipHandle;
        else if (tipKey >= 2000) t = kTipPrev + (tipKey - 2000);
        else { const int hh = tipKey - 1000;
            t = hh == 1 ? kTipLearn : hh == 2 ? kTipMidi : hh == 3 ? kTipUndo : hh == 4 ? kTipRedo : hh == 5 ? kTipHelp : hh == 6 ? kTipA : hh == 7 ? kTipB : kTipCopy; }
        title = targetTitle(t); value.clear(); body = targetTip(t);
        if (t == kTipUndo || t == kTipRedo) { const size_t n = t == kTipUndo ? undoStack.size() : redoStack.size(); char b[32]; snprintf(b, sizeof b, "%d step%s", (int)n, n == 1 ? "" : "s"); value = b; }
        if (t == kTipHandle) { char a[24], b[24]; fmt(kFreq, a, sizeof a); fmt(kPeak, b, sizeof b); value = std::string(a) + "  ·  " + b; }
        if (t == kTipEfLight) { char b[32]; snprintf(b, sizeof b, "%+.2f oct", val[kOutEf]); value = b; }
        return true;
    }
    void drawTooltip() {
        std::string title, value, body;
        if (!tipContent(title, value, body)) return;
        const float w = 300, pad = 12;
        fontSize(11); textLetterSpacing(0.2f); textLineHeight(1.3f);
        float bb[4]; textBoxBounds(0, 0, w - 2 * pad, body.c_str(), nullptr, bb);
        const float h = pad + 16 + 8 + (bb[3] - bb[1]) + pad;
        float x = tipX + 16, y = tipY + 20;
        if (x + w > W - 8) x = tipX - w - 12;
        if (y + h > H - 8) y = tipY - h - 12;
        x = clampf(x, 8, W - w - 8); y = clampf(y, 8, H - h - 8);
        beginPath(); rect(x - 10, y - 4, w + 20, h + 18); fillPaint(boxGradient(x, y + 3, w, h, 6, 14, Color(0, 0, 0, 0.55f), Color(0, 0, 0, 0.f))); fill();
        beginPath(); roundedRect(x, y, w, h, 6); fillColor(Color(0x1e, 0x1e, 0x21)); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
        fontSize(10); textLetterSpacing(1.6f); textAlign(ALIGN_LEFT | ALIGN_TOP); fillColor(C_TEXT);
        std::string up = title; for (char& ch : up) ch = (char)std::toupper((unsigned char)ch);
        text(x + pad, y + pad + 1, up.c_str(), nullptr);
        if (!value.empty()) { fontSize(10.5f); textLetterSpacing(0.4f); textAlign(ALIGN_RIGHT | ALIGN_TOP); fillColor(C_ACC); text(x + w - pad, y + pad + 1, value.c_str(), nullptr); }
        fontSize(11); textLetterSpacing(0.2f); textLineHeight(1.3f); textAlign(ALIGN_LEFT | ALIGN_TOP); fillColor(C_DIM);
        textBox(x + pad, y + pad + 24, w - 2 * pad, body.c_str(), nullptr);
        textLineHeight(1.f);
    }

    // ---- help window ---------------------------------------------------------------
    static constexpr float HX = 70, HY = 50, HW = 760, HH = 500, TAB_W = 170, TAB_H = 34;
    // hit ids: 0..pages-1 tabs, 100 tips toggle, 101 manual, 102 close, -1 inside, -2 outside
    int helpHit(float x, float y) const {
        if (x < HX || x > HX + HW || y < HY || y > HY + HH) return -2;
        for (int i = 0; i < kNumHelpPages; ++i) if (x >= HX + 12 && x < HX + 12 + TAB_W && y >= HY + 56 + i * TAB_H && y < HY + 56 + (i + 1) * TAB_H) return i;
        const float by = HY + HH - 44;
        if (y >= by && y < by + 28) {
            if (x >= HX + 12 && x < HX + 12 + TAB_W) return 100;
            if (x >= HX + HW - 330 && x < HX + HW - 140) return 101;
            if (x >= HX + HW - 124 && x < HX + HW - 16) return 102;
        }
        if (x >= HX + HW - 40 && y < HY + 40) return 102;
        return -1;
    }
    void helpClick(float x, float y) {
        const int h = helpHit(x, y);
        if (h >= 0 && h < kNumHelpPages) helpPage = h;
        else if (h == 100) gTipsOn = !gTipsOn;
        else if (h == 101) openURL(kManualURL);
        else if (h == 102 || h == -2) helpOpen = false;
        repaint();
    }
    void drawHelp() {
        beginPath(); rect(0, 0, W, H); fillColor(Color(0, 0, 0, 0.55f)); fill();
        beginPath(); roundedRect(HX, HY, HW, HH, 10); fillColor(C_PANEL); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
        fontSize(13); textLetterSpacing(4.f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(C_TEXT); text(HX + 22, HY + 28, "FLINT HELP", nullptr);
        const float cx = HX + HW - 24, cy = HY + 26;
        beginPath(); moveTo(cx - 6, cy - 6); lineTo(cx + 6, cy + 6); moveTo(cx + 6, cy - 6); lineTo(cx - 6, cy + 6); strokeColor(helpHover == 102 ? C_TEXT : C_DIM); strokeWidth(1.6f); stroke();
        for (int i = 0; i < kNumHelpPages; ++i) {
            const float ty = HY + 56 + i * TAB_H;
            if (i == helpPage || i == helpHover) { beginPath(); roundedRect(HX + 12, ty + 2, TAB_W, TAB_H - 4, 5); fillColor(i == helpPage ? withAlpha(C_ACC, 0.2f) : C_PANEL_HI); fill(); }
            if (i == helpPage) { beginPath(); roundedRect(HX + 12, ty + 8, 3, TAB_H - 16, 1.5f); fillColor(C_ACC); fill(); }
            fontSize(11.5f); textLetterSpacing(0.6f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(i == helpPage ? C_TEXT : C_DIM);
            text(HX + 28, ty + TAB_H / 2 + 0.5f, kHelpPages[i].title, nullptr);
        }
        beginPath(); moveTo(HX + TAB_W + 26, HY + 56); lineTo(HX + TAB_W + 26, HY + HH - 56); strokeColor(C_LINE); strokeWidth(1); stroke();
        const float bx = HX + TAB_W + 46, bw = HW - TAB_W - 70;
        fontSize(15); textLetterSpacing(0.8f); textAlign(ALIGN_LEFT | ALIGN_TOP); fillColor(C_ACC); text(bx, HY + 60, kHelpPages[helpPage].title, nullptr);
        fontSize(12); textLetterSpacing(0.2f); textLineHeight(1.38f); fillColor(Color(0xc6, 0xc4, 0xc0));
        textBox(bx, HY + 92, bw, kHelpPages[helpPage].body, nullptr);
        textLineHeight(1.f);
        const float by = HY + HH - 44;
        beginPath(); moveTo(HX + 12, by - 10); lineTo(HX + HW - 12, by - 10); strokeColor(C_LINE); stroke();
        const float tw = 34, th = 18, tx = HX + 16, tyy = by + 14 - th / 2;
        beginPath(); roundedRect(tx, tyy, tw, th, th / 2); fillColor(gTipsOn ? withAlpha(C_ACC, 0.25f) : C_PANEL_HI); fill(); strokeColor(gTipsOn ? C_ACC : C_LINE); stroke();
        beginPath(); circle(gTipsOn ? tx + tw - th / 2 : tx + th / 2, by + 14, 6); fillColor(gTipsOn ? C_ACC : C_DIM); fill();
        fontSize(10); textLetterSpacing(1.6f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(helpHover == 100 ? C_TEXT : C_DIM); text(tx + tw + 10, by + 14.5f, "HOVER TIPS", nullptr);
        auto btn = [&](float x0, float w0, const char* label, bool hot, bool primary) {
            beginPath(); roundedRect(x0, by, w0, 28, 5); fillColor(primary ? (hot ? withAlpha(C_ACC, 0.35f) : withAlpha(C_ACC, 0.18f)) : (hot ? C_PANEL_HI : C_PANEL)); fill();
            strokeColor(primary ? C_ACC : C_LINE); strokeWidth(1); stroke();
            fontSize(9.5f); textLetterSpacing(1.8f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_TEXT); text(x0 + w0 / 2, by + 14.5f, label, nullptr);
        };
        btn(HX + HW - 330, 190, "FULL MANUAL (WEB)", helpHover == 101, false);
        btn(HX + HW - 124, 108, "CLOSE", helpHover == 102, true);
    }

private:
    float val[kParamCount], defv[kParamCount], userVals[kParamCount] = {}, before[kParamCount];
    int paramCC[kParamCount];
    int active = -1, hover = -1, lastClick = -1, editing = -1;
    uint lastClickTime = 0;
    float dragY = 0, dragNorm = 0; bool circular = false, gesture = false;
    bool dragHandle = false, hoverHandle = false;
    float dragX0 = 0, dragY0 = 0, startOct = 10, startPeak = 0;
    std::vector<Snap> undoStack, redoStack;
    Snap abStore[2]; int abSlot = 0;
    std::string editBuf, statusText; int statusFlash = 0;
    int curFactory = 0, presetHover = -1, menuHover = -1, pendingDelete = -1, hoverHeader = 0;
    std::string curUser; std::vector<std::string> userPresets;
    bool menuOpen = false, saving = false, saveError = false; std::string saveName;
    bool learnMode = false, hoverClear = false; int learnWaiting = -1; float lastLearnEcho = 0;
    int tipKey = -1, tipSuppressed = -2; double tipSince = 0; bool tipShown = false; float tipX = 0, tipY = 0;
    bool helpOpen = false; int helpPage = 0, helpHover = -1;
    ResizeHandle resizeHandle;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FlintUI)
};

UI* createUI() { return new FlintUI(); }

END_NAMESPACE_DISTRHO
