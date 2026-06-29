#include "AppCharles.h"
#include "widgets/brace/brace.h"
#include "widgets/icon_button/icon_button.h"
#include "widgets/curve_chart/curve_chart.h"
#include "blinker/Blinker.h"
#include "core/coroutine/Coroutine.h"
#include "ui/ListView/ListView.h"
#include "HardwareManager.h"
#include <cstring>
#include <cstdio>
#include <cmath>

static const unsigned char image_charles_bits[] = {
  0xF0, 0xFF, 0x0F, 0xFC, 0xFF, 0x3F, 0xFE, 0x00, 0x7F, 0xBE, 0x7E, 0x7E,
  0xBF, 0xFE, 0xFC, 0x0F, 0xF8, 0xF9, 0x0F, 0xF8, 0xFB, 0x1F, 0xFC, 0xE0,
  0xEF, 0x7B, 0xDF, 0xF7, 0xB7, 0xAF, 0x5B, 0xAF, 0xB7, 0x8B, 0xAF, 0xBB,
  0xDB, 0xA8, 0xBB, 0x8B, 0x6A, 0xDF, 0xDB, 0xEE, 0xE0, 0x8B, 0xEA, 0xFF,
  0xDB, 0x28, 0xFC, 0xAB, 0xAF, 0xFD, 0xAB, 0x2D, 0xFC, 0xDB, 0xA8, 0xF7,
  0xF2, 0xA7, 0x63, 0x06, 0xB0, 0x41, 0xFC, 0xFF, 0x3F, 0xF0, 0xFF, 0x0F
};

static const unsigned char next_arrow_bits[] = {
    0x10, 0x30, 0x70, 0xF0, 0x70, 0x30, 0x10
};

// Experiment config
static bool    g_autoMode     = true;
static bool    g_unitIsKpa    = false;
static bool    g_tempIsK      = false;
static int32_t g_targetVolume = 250;   // mL, isochoric target

// MANUAL mode temp steps
static int32_t g_manualSteps[CharlesLaw::MAX_STEPS] = {};
static int32_t g_stepCount = 0;

// ===== MANUAL Step sub-menu =====
class AppCharlesStepMenu : public ListView {
private:
    ListItem m_items[11];

    static void setTitle(ListItem& item, const char* t) {
        strncpy(item.title, t, sizeof(item.title) - 1);
        item.title[sizeof(item.title) - 1] = '\0';
    }

    static void clearItem(ListItem& item) {
        item.extra          = {nullptr, nullptr, nullptr, nullptr};
        item.nextList       = nullptr;
        item.nextListLength = 0;
        item.pFunc          = nullptr;
    }

    void rebuildItems() {
        clearItem(m_items[0]);
        setTitle(m_items[0], ">>> 温度节点 <<<");

        for (int32_t i = 0; i < g_stepCount; i++) {
            clearItem(m_items[i + 1]);
            char buf[12];
            snprintf(buf, sizeof(buf), "节点%d", (int)(i + 1));
            setTitle(m_items[i + 1], buf);
            m_items[i + 1].extra.intValue = &g_manualSteps[i];
            int32_t idx = i;
            m_items[i + 1].pFunc = [this, idx]() {
                m_ui.showPopupProgress(g_manualSteps[idx], 25, 80,
                    "温度(C)", 100, 40, 5000, 1, nullptr, true, 5);
            };
        }

        int32_t base = g_stepCount + 1;

        clearItem(m_items[base]);
        if (g_stepCount < (int32_t)CharlesLaw::MAX_STEPS) {
            setTitle(m_items[base], "添加节点");
            m_items[base].pFunc = [this]() {
                if (g_stepCount >= (int32_t)CharlesLaw::MAX_STEPS) return;
                g_manualSteps[g_stepCount] = 30;
                g_stepCount++;
                rebuildItems();
                setCursor(g_stepCount + 1);
            };
        } else {
            setTitle(m_items[base], "(节点已满)");
        }

        clearItem(m_items[base + 1]);
        setTitle(m_items[base + 1], "删除末尾");
        m_items[base + 1].pFunc = [this]() {
            if (g_stepCount == 0) return;
            g_stepCount--;
            rebuildItems();
            setCursor(g_stepCount + 2);
        };

        clearItem(m_items[base + 2]);
        setTitle(m_items[base + 2], "清空全部");
        m_items[base + 2].pFunc = [this]() {
            g_stepCount = 0;
            rebuildItems();
            setCursor(3);
        };

        resizeLength(g_stepCount + 3);
    }

public:
    AppCharlesStepMenu(PixelUI& ui) : ListView(ui, m_items, 11) {}

    void onEnter(ExitCallback cb) override {
        rebuildItems();
        ListView::onEnter(cb);
    }

    void onLoad() override {}
    void onSave() override {}
    void onExit() override {}
};

// ===== Settings ListView =====
class AppCharlesSettings : public ListView {
private:
    ListItem            m_items[8];
    AppCharlesStepMenu  m_stepMenu;
    bool                m_inStepMenu  = false;
    bool                g_valveOpen   = false;

    static void setTitle(ListItem& item, const char* t) {
        strncpy(item.title, t, sizeof(item.title) - 1);
        item.title[sizeof(item.title) - 1] = '\0';
    }

    static void clearItem(ListItem& item) {
        item.extra          = {nullptr, nullptr, nullptr, nullptr};
        item.nextList       = nullptr;
        item.nextListLength = 0;
        item.pFunc          = nullptr;
    }

    void rebuildModeItems() {
        clearItem(m_items[4]);
        if (g_autoMode) {
            setTitle(m_items[4], "温度节点");
            m_items[4].pFunc = [this]() {
                m_ui.showPopupInfo(
                    "30>35>40>45\n50>55>60 C",
                    "AUTO 节点", 110, 40, 3000);
            };
        } else {
            setTitle(m_items[4], "温度节点");
            m_items[4].extra.intValue = &g_stepCount;
            m_items[4].pFunc = [this]() {
                m_inStepMenu = true;
                m_stepMenu.onEnter([this]() {
                    m_inStepMenu = false;
                });
            };
        }

        clearItem(m_items[5]);
        setTitle(m_items[5], "气阀");
        m_items[5].extra.switchValue = &g_valveOpen;
        m_items[5].pFunc = [this]() {
            if (g_valveOpen) charles.openValve();
            else             charles.closeValve();
        };

        clearItem(m_items[6]);
        setTitle(m_items[6], "开始实验");
        m_items[6].pFunc = [this]() {
            if (onStartExperiment) onStartExperiment();
        };

        clearItem(m_items[7]);
        setTitle(m_items[7], "实验报告");
        m_items[7].pFunc = [this]() {
            if (onOpenReport) onOpenReport();
        };

        resizeLength(7);
    }

    void setupItems() {
        setTitle(m_items[0], ">>> 实验设置 <<<");

        setTitle(m_items[1], "自动模式");
        m_items[1].extra.switchValue = &g_autoMode;
        m_items[1].pFunc = [this]() { rebuildModeItems(); };

        setTitle(m_items[2], "压强单位 kPa");
        m_items[2].extra.switchValue = &g_unitIsKpa;

        setTitle(m_items[3], "目标体积");
        m_items[3].extra.intValue = &g_targetVolume;
        m_items[3].pFunc = [this]() {
            m_ui.showPopupProgress(g_targetVolume, 100, 450,
                "目标体积(mL)", 100, 40, 4000, 1, nullptr, true, 25);
        };

        rebuildModeItems();
    }

public:
    std::function<void()> onStartExperiment;
    std::function<void()> onOpenReport;

    AppCharlesSettings(PixelUI& ui) : ListView(ui, m_items, 8), m_stepMenu(ui) {
        setupItems();
    }

    void draw() override {
        if (m_inStepMenu) m_stepMenu.draw();
        else              ListView::draw();
    }

    bool handleInput(InputEvent event) override {
        if (m_inStepMenu) return m_stepMenu.handleInput(event);
        return ListView::handleInput(event);
    }

    void reattachFocus() {
        g_valveOpen = charles.isValveOpen();
        m_ui.markDirty();
    }

    void onLoad() override { g_valveOpen = charles.isValveOpen(); }
    void onSave() override {}
    void onExit() override {}
};

// Running page status icons (7x7)
static const unsigned char pause_icon_bits[] = { 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36 };

// ===== Running page =====
class AppCharlesRunning {
private:
    PixelUI&   m_ui;
    Brace      m_brace;
    CurveChart m_histogram;
    Blinker    m_blinker;
    Coroutine  m_entryCoro;
    IconButton m_reportBtn;

    uint8_t  m_lastDataCount = 0;
    uint32_t m_prevTime      = 0;
    char     m_buf[24]       = {};

    int32_t anim_bg       = 0;
    int32_t anim_vol_x    = -30;
    int32_t anim_mark_m   = 0;
    int32_t anim_status_x = -27;

    std::function<void()> m_onExit;

    void entryCoro(CoroutineContext& ctx) {
        CORO_BEGIN(ctx);
            m_ui.animate(anim_bg,    128, 400, EasingType::EASE_OUT_CUBIC, PROTECTION::PROTECTED);
            m_ui.animate(anim_vol_x,   0, 350, EasingType::EASE_OUT_CUBIC, PROTECTION::PROTECTED);
        CORO_DELAY(ctx, m_ui, 420, 10);
            m_brace.onLoad();
        CORO_DELAY(ctx, m_ui, 80, 20);
            m_ui.addWidgetToFocusManager(&m_brace);
            m_ui.addWidgetToFocusManager(&m_histogram);
            m_ui.addWidgetToFocusManager(&m_reportBtn);
            m_ui.animate(anim_mark_m,   23, 300, EasingType::EASE_OUT_QUAD,  PROTECTION::PROTECTED);
            m_ui.animate(anim_status_x, 38, 450, EasingType::EASE_OUT_CUBIC, PROTECTION::PROTECTED);
        CORO_END(ctx);
    }

    struct StateInfo {
        const char* tag;
        const char* desc;
        uint32_t    blinkMs;
    };

    StateInfo stateInfo() {
        switch (charles.getState()) {
            case CharlesLaw::INIT_VOLUME: return {"Init",  "INITVOL", 0};
            case CharlesLaw::STEPPING:   return {"Heat",  "HEATING", 500};
            case CharlesLaw::STABILIZING:return {"Stab",  "STABLE",  500};
            case CharlesLaw::RECORDING:  return {"Rec",   "RECORD",  150};
            case CharlesLaw::DONE:       return {"Done",  "DONE",    0};
            case CharlesLaw::ERROR:      return {"Err",   "ERROR",   300};
            default:                     return {"Idle",  "IDLE",    0};
        }
    }

    // ---- Brace: current temp / next target temp ----
    enum class BracePage { CURRENT = 0, NEXT_TARGET = 1 };
    BracePage m_bracePage       = BracePage::CURRENT;
    BracePage m_bracePageTarget = BracePage::CURRENT;
    int32_t   m_braceAnimY      = 0;
    bool      m_braceAnimating  = false;

    void braceCallback() {
        if (m_braceAnimating) return;
        m_bracePageTarget = (m_bracePage == BracePage::CURRENT)
                          ? BracePage::NEXT_TARGET
                          : BracePage::CURRENT;
        m_braceAnimY = 0;
        m_ui.animate(m_braceAnimY, 18, 300, EasingType::EASE_IN_OUT_CUBIC, PROTECTION::PROTECTED);
        m_braceAnimating = true;
    }

    void braceContent() {
        U8G2& u8g2 = m_ui.getU8G2();
        u8g2.setFont(u8g2_font_5x7_tr);
        const int PH = 18;

        if (m_braceAnimating && m_braceAnimY >= 17) {
            m_bracePage      = m_bracePageTarget;
            m_braceAnimY     = 0;
            m_braceAnimating = false;
        }

        auto drawPage = [&](BracePage page, int yBase) {
            if (yBase < 45 || yBase > 70) return;

            u8g2.drawRBox(8, yBase - 8, 24, 10, 2);
            u8g2.setDrawColor(0);

            if (page == BracePage::CURRENT) {
                u8g2.drawStr(10, yBase, "Temp");
                u8g2.setDrawColor(1);
                float t = bme.getTemperature();
                if (g_tempIsK) snprintf(m_buf, sizeof(m_buf), isnan(t) ? "--" : "%.0f", t + 273.15f);
                else           snprintf(m_buf, sizeof(m_buf), isnan(t) ? "--" : "%.1f", t);
                int tw = u8g2.getStrWidth(m_buf);
                u8g2.drawStr(58 - tw, yBase - 4, m_buf);
                u8g2.drawStr(50, yBase + 3, g_tempIsK ? "K" : "C");
            } else {
                u8g2.drawStr(10, yBase, "Next");
                u8g2.setDrawColor(1);
                CharlesLaw::State s = charles.getState();
                bool running = (s == CharlesLaw::STEPPING || s == CharlesLaw::STABILIZING
                             || s == CharlesLaw::RECORDING);
                float t = running ? charles.getNextTargetTemp()
                                  : charles.getCurrentTargetTemp();
                if (t >= 0.0f) {
                    snprintf(m_buf, sizeof(m_buf), "%.0f", t);
                    int tw = u8g2.getStrWidth(m_buf);
                    u8g2.drawStr(58 - tw, yBase - 4, m_buf);
                } else {
                    u8g2.drawStr(36, yBase - 4, "End");
                }
                u8g2.drawStr(50, yBase + 3, "C");
            }
        };

        int cy = 58 + m_braceAnimY;
        int ny = 58 + m_braceAnimY - PH;
        drawPage(m_bracePage,       cy);
        drawPage(m_bracePageTarget, ny);
    }

    void drawStatusBar() {
        U8G2& u8g2 = m_ui.getU8G2();
        u8g2.setFont(u8g2_font_5x7_tr);

        // Left: current air volume
        float v = volumeCtrl.getAirVolume();
        if (isnan(v)) snprintf(m_buf, sizeof(m_buf), "V=-- mL");
        else          snprintf(m_buf, sizeof(m_buf), "V=%.0fmL", v);
        u8g2.drawStr(anim_vol_x, 7, m_buf);

        // Center: step progress bar
        uint8_t total = charles.getTotalSteps();
        if (total > 0) {
            uint8_t cur = charles.getCurrentStep();
            snprintf(m_buf, sizeof(m_buf), "%d/%d", cur, total);
            int tw    = u8g2.getStrWidth(m_buf);
            int bar_x = 38;
            int bar_w = 110 - bar_x - tw - 3;
            int bar_y = 2;
            int bar_h = 5;
            int filled = (int)((float)cur / total * bar_w);
            u8g2.drawFrame(bar_x, bar_y, bar_w, bar_h);
            if (filled > 0) u8g2.drawBox(bar_x, bar_y, filled, bar_h);
            u8g2.drawStr(bar_x + bar_w + 3, 7, m_buf);
        }

        // Right: pause icon
        u8g2.drawXBMP(119, 1, 7, 7, pause_icon_bits);

        // Divider
        u8g2.setClipWindow(0, 9, anim_bg, 10);
        u8g2.drawHLine(0, 9, 128);
        u8g2.setMaxClipWindow();
    }

    void drawMainArea() {
        U8G2& u8g2 = m_ui.getU8G2();

        // Main pressure value
        u8g2.setFont(u8g2_font_profont17_tr);
        float p = bme.getPressure();
        if (isnan(p)) {
            snprintf(m_buf, sizeof(m_buf), g_unitIsKpa ? "---.- kPa" : "---.- hPa");
        } else if (!g_unitIsKpa) {
            snprintf(m_buf, sizeof(m_buf), "%.1f hPa", p);
        } else {
            snprintf(m_buf, sizeof(m_buf), "%.2f kPa", p / 10.0f);
        }
        u8g2.drawStr(3, 30, m_buf);

        // State block
        StateInfo si = stateInfo();
        if (si.blinkMs > 0) {
            m_blinker.set_interval(si.blinkMs);
            m_blinker.start();
        } else {
            m_blinker.stopOnVisible();
        }

        u8g2.setFont(u8g2_font_5x7_tr);
        bool shouldDraw = (si.blinkMs == 0) || m_blinker.is_visible();
        if (shouldDraw) {
            u8g2.setDrawColor(1);
            u8g2.drawStr(5, 42, si.tag);
            u8g2.setDrawColor(2);
            u8g2.drawBox(3, 35, anim_mark_m, 8);
            u8g2.setDrawColor(1);
            int clip_x = anim_mark_m + 14;
            u8g2.setClipWindow(clip_x, 36, 128, 43);
            u8g2.drawStr(anim_status_x, 42, si.desc);
            u8g2.setMaxClipWindow();
        }

        u8g2.setFont(u8g2_font_5x7_tr);
        u8g2.drawStr(108, 42, "RPT");
        m_reportBtn.draw();
    }

public:
    std::function<void()> onReport;

    AppCharlesRunning(PixelUI& ui)
        : m_ui(ui)
        , m_brace(ui, 3, 45, 56, 18)
        , m_histogram(ui, 69, 45, 56, 18, 76, 63, EXPAND_BASE::BOTTOM_RIGHT, (char*)"Pres")
        , m_blinker(ui, 500)
        , m_entryCoro([this](CoroutineContext& ctx){ entryCoro(ctx); })
        , m_reportBtn(ui, 98, 36, 8, 7, next_arrow_bits)
    {}

    void reattachFocus() {
        m_ui.addWidgetToFocusManager(&m_brace);
        m_ui.addWidgetToFocusManager(&m_histogram);
        m_ui.addWidgetToFocusManager(&m_reportBtn);
    }

    void enter(std::function<void()> onExit) {
        m_onExit         = onExit;
        m_lastDataCount  = 0;
        m_prevTime       = m_ui.getCurrentTime();
        anim_bg          = 0;
        anim_vol_x       = -30;
        anim_mark_m      = 0;
        anim_status_x    = -27;

        m_blinker.stopOnVisible();
        m_bracePage       = BracePage::CURRENT;
        m_bracePageTarget = BracePage::CURRENT;
        m_braceAnimY      = 0;
        m_braceAnimating  = false;

        m_brace.setDrawContentFunction([this]() { braceContent(); });
        m_brace.setCallback([this]() { braceCallback(); });

        m_reportBtn.onLoad();
        m_reportBtn.setCallback([this]() {
            if (onReport) onReport();
        });

        m_histogram.onLoad();

        m_entryCoro.reset();
        m_entryCoro.start();
        m_ui.addCoroutine(&m_entryCoro);

        m_ui.setContinousDraw(true);
        m_ui.markDirty();
    }

    void draw() {
        m_blinker.update();

        uint32_t now = m_ui.getCurrentTime();
        if (now - m_prevTime >= 500) {
            m_prevTime = now;
            m_histogram.addData(bme.getPressure());
        }

        if (m_histogram.isExpanded()) {
            U8G2& u8g2 = m_ui.getU8G2();
            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_5x7_tr);
            u8g2.drawStr(0, 8, "<STATUS>");

            float pmax = m_histogram.getMaxValueInHistory();
            float pavg = m_histogram.getAverageValueInHistory();

            u8g2.drawStr(0, 20, "Max:");
            if (!g_unitIsKpa) snprintf(m_buf, sizeof(m_buf), "%.1f hPa", pmax);
            else              snprintf(m_buf, sizeof(m_buf), "%.2f kPa", pmax / 10.0f);
            u8g2.drawStr(0, 30, m_buf);

            u8g2.drawStr(0, 42, "Avg:");
            if (!g_unitIsKpa) snprintf(m_buf, sizeof(m_buf), "%.1f hPa", pavg);
            else              snprintf(m_buf, sizeof(m_buf), "%.2f kPa", pavg / 10.0f);
            u8g2.drawStr(0, 52, m_buf);

            m_histogram.draw();
            return;
        }

        drawStatusBar();
        drawMainArea();
        m_brace.draw();
        m_histogram.draw();
    }

    bool handleInput(InputEvent event) {
        if (event == InputEvent::BACK) {
            charles.stop();
            charles.reset();
            m_ui.removeCoroutine(&m_entryCoro);
            m_ui.clearFocusManager();
            if (m_onExit) m_onExit();
            return true;
        }
        m_ui.handleInput(event);
        return true;
    }
};

enum class CharlesState { MAIN_IDLE, NEXT_PAGE, RUNNING, REPORT };
enum class CharlesLoadState { INIT, BRACE_LOADING, DONE };

// ===== Report page =====
class AppCharlesReport {
private:
    PixelUI& m_ui;
    int      m_scrollOffset = 0;
    int      m_scrollAnim   = 0;
    int      m_scrollTarget = 0;
    char     m_buf[24]      = {};

    std::function<void()> m_onExit;

    // 快照数据（实验结束/退出后持久保留）
    CharlesLaw::DataPoint m_snapData[CharlesLaw::MAX_STEPS] = {};
    uint8_t               m_snapCount       = 0;
    float                 m_snapLockedVolume = 0.0f;
    bool                  m_snapUnitKpa     = false;
    bool                  m_snapTempIsK     = false;

    static const int ROW_H   = 9;
    static const int VISIBLE = 5;
    static const int HEADER_Y = 8;
    static const int DATA_Y0  = 17;
    static const int SCROLL_X = 125;

    void drawRowAt(int rowIndex, int yBase, int xOff) {
        U8G2& u8g2 = m_ui.getU8G2();
        u8g2.setFont(u8g2_font_5x7_tr);

        if (rowIndex < (int)m_snapCount) {
            const auto& dp = m_snapData[rowIndex];

            // # 列
            snprintf(m_buf, sizeof(m_buf), "%d", rowIndex + 1);
            u8g2.drawStr(xOff + 0, yBase, m_buf);

            // P 列
            if (!g_unitIsKpa) snprintf(m_buf, sizeof(m_buf), "%.1f", dp.P);
            else              snprintf(m_buf, sizeof(m_buf), "%.2f", dp.P / 10.0f);
            int tw = u8g2.getStrWidth(m_buf);
            u8g2.drawStr(xOff + 50 - tw, yBase, m_buf);

            // T 列
            if (!g_tempIsK) snprintf(m_buf, sizeof(m_buf), "%.1f", dp.T - 273.15f);
            else            snprintf(m_buf, sizeof(m_buf), "%.1f", dp.T);
            tw = u8g2.getStrWidth(m_buf);
            u8g2.drawStr(xOff + 88 - tw, yBase, m_buf);

            // P/T 列
            snprintf(m_buf, sizeof(m_buf), "%.4f", dp.PT);
            tw = u8g2.getStrWidth(m_buf);
            u8g2.drawStr(xOff + 122 - tw, yBase, m_buf);
        }
    }

    void drawScrollbarAt(int total, int xOff) {
        if (total <= VISIBLE) return;
        U8G2& u8g2 = m_ui.getU8G2();
        const int TRACK_TOP = 9;
        const int TRACK_H   = 44;
        int seg_h = std::max(4, TRACK_H * VISIBLE / total);
        int seg_y = TRACK_TOP + (TRACK_H - seg_h) * m_scrollOffset / std::max(1, total - VISIBLE);
        u8g2.drawVLine(xOff + SCROLL_X, seg_y, seg_h);
    }

public:
    AppCharlesReport(PixelUI& ui) : m_ui(ui) {}

    void saveSnapshot() {
        m_snapCount   = charles.getDataCount();
        m_snapUnitKpa = g_unitIsKpa;
        m_snapTempIsK = g_tempIsK;
        m_snapLockedVolume = charles.getLockedVolume();
        for (uint8_t i = 0; i < m_snapCount; i++)
            m_snapData[i] = charles.getDataPoint(i);
    }
    bool hasSnapshot() const { return m_snapCount > 0; }

    void enter(std::function<void()> onExit) {
        m_onExit       = onExit;
        m_scrollOffset = 0;
        m_scrollAnim   = 0;
        m_scrollTarget = 0;
        m_ui.setContinousDraw(true);
        m_ui.markDirty();
    }

    void draw()            { drawAt(0); }
    void drawAt(int xOff) {
        if (m_scrollAnim != m_scrollTarget) {
            int diff = m_scrollTarget - m_scrollAnim;
            m_scrollAnim += (diff > 0 ? 1 : -1) * std::max(1, abs(diff) / 2 + 1);
            if (abs(m_scrollTarget - m_scrollAnim) <= 1) m_scrollAnim = m_scrollTarget;
        }

        U8G2& u8g2 = m_ui.getU8G2();

        u8g2.setDrawColor(0);
        u8g2.drawBox(xOff, 0, 128, 64);
        u8g2.setDrawColor(1);

        u8g2.setFont(u8g2_font_5x7_tr);

        // 表头
        u8g2.drawStr(xOff + 0,  HEADER_Y, "#");
        const char* pUnit = g_unitIsKpa ? "kPa" : "hPa";
        u8g2.drawStr(xOff + 20, HEADER_Y, pUnit);
        const char* tUnit = g_tempIsK ? "T(K)" : "T(C)";
        u8g2.drawStr(xOff + 60, HEADER_Y, tUnit);
        u8g2.drawStr(xOff + 90, HEADER_Y, "P/T");
        u8g2.drawHLine(xOff, 9, 123);

        // 数据行（带像素滚动）
        u8g2.setClipWindow(xOff, 10, xOff + 123, 53);
        int cnt = (int)m_snapCount;
        for (int i = 0; i < VISIBLE + 1; i++) {
            int rowIndex = m_scrollOffset + i;
            int yBase    = DATA_Y0 + i * ROW_H - (m_scrollAnim % ROW_H);
            if (yBase > 63) break;
            drawRowAt(rowIndex, yBase, xOff);
        }
        u8g2.setMaxClipWindow();

        drawScrollbarAt(cnt, xOff);

        // 底栏
        u8g2.drawHLine(xOff, 54, 123);
        if (cnt > 0) {
            // 锁定体积
            snprintf(m_buf, sizeof(m_buf), "V:%.0fmL", m_snapLockedVolume);
            u8g2.drawStr(xOff, 63, m_buf);

            if (cnt > 1) {
                float ptSum = 0;
                for (int i = 0; i < cnt; i++) ptSum += m_snapData[i].PT;
                float ptMean = ptSum / cnt;
                float errSum = 0;
                for (int i = 0; i < cnt; i++)
                    errSum += fabsf(m_snapData[i].PT - ptMean) / ptMean * 100.0f;
                snprintf(m_buf, sizeof(m_buf), "Err:%.2f%%", errSum / cnt);
                u8g2.drawStr(xOff + 40, 63, m_buf);
            } else {
                u8g2.drawStr(xOff + 40, 63, "Err:---");
            }
        } else {
            u8g2.drawStr(xOff,      63, "V:---  ");
            u8g2.drawStr(xOff + 40, 63, "Err:---");
        }
    }

    bool handleInput(InputEvent event) {
        int cnt       = (int)m_snapCount;
        int maxOffset = std::max(0, cnt - VISIBLE);
        if (event == InputEvent::BACK) {
            m_ui.clearFocusManager();
            if (m_onExit) m_onExit();
            return true;
        }
        if (event == InputEvent::DOWN || event == InputEvent::RIGHT) {
            if (m_scrollOffset < maxOffset) {
                m_scrollOffset++;
                m_scrollTarget = m_scrollOffset * ROW_H;
                m_ui.markDirty();
            }
            return true;
        }
        if (event == InputEvent::UP || event == InputEvent::LEFT) {
            if (m_scrollOffset > 0) {
                m_scrollOffset--;
                m_scrollTarget = m_scrollOffset * ROW_H;
                m_ui.markDirty();
            }
            return true;
        }
        return true;
    }
};

class AppCharles : public IApplication {
private:
    PixelUI&           m_ui;
    CharlesState       state;
    CharlesLoadState   loadState;
    AppCharlesSettings m_settings;
    AppCharlesRunning  m_running;
    AppCharlesReport   m_report;

    int32_t m_slideX             = 0;
    bool    m_slidingIn          = false;
    bool    m_reportFromSettings = false;

    Brace      volumeBrace;
    Brace      tempBrace;
    IconButton nextButton;

    bool    first_time    = false;
    int32_t anim_status_x = 10;
    int32_t anim_mode_box = 0;
    int32_t anim_bg       = 0;
    float   pressure      = 0.0f;

    char m_buf[32] = {};

    // Volume brace flip
    enum class VolumePage { VOL_A = 0, VOL_L = 1 };
    VolumePage currentVolPage = VolumePage::VOL_A;
    VolumePage targetVolPage  = VolumePage::VOL_A;
    int32_t    anim_vol_y     = 0;
    bool       vol_animating  = false;

    // Temp brace flip
    bool    currentTempUnit = true;   // true = °C
    bool    targetTempUnit  = true;
    int32_t anim_temp_y     = 0;
    bool    temp_animating  = false;

    void volumeBraceContent() {
        U8G2& u8g2 = m_ui.getU8G2();
        u8g2.setFont(u8g2_font_5x7_tr);
        const int PAGE_HEIGHT = 18;
        float volAir = volumeCtrl.getAirVolume();
        float volLiq = volumeCtrl.getLiquidVolume();

        auto drawPage = [&](VolumePage page, int y_base) {
            if (y_base < 45 || y_base > 70) return;
            const char* label = (page == VolumePage::VOL_A) ? "VolA" : "VolL";
            float value       = (page == VolumePage::VOL_A) ? volAir : volLiq;
            u8g2.drawRBox(8, y_base - 8, 24, 10, 2);
            u8g2.setDrawColor(0);
            u8g2.drawStr(10, y_base, label);
            u8g2.setDrawColor(1);
            char buf[16];
            snprintf(buf, sizeof(buf), "%.0f", value);
            int tw = u8g2.getStrWidth(buf);
            u8g2.drawStr(58 - tw, y_base - 4, buf);
            u8g2.drawStr(50, y_base + 3, "mL");
        };

        int cy = 58 + anim_vol_y;
        int ny = 58 + anim_vol_y - PAGE_HEIGHT;
        drawPage(currentVolPage, cy);
        drawPage(targetVolPage,  ny);
    }

    void tempBraceContent() {
        U8G2& u8g2 = m_ui.getU8G2();
        u8g2.setFont(u8g2_font_5x7_tr);
        const int PAGE_HEIGHT = 18;
        float temp = bme.getTemperature();

        auto drawPage = [&](bool celsius, int y_base) {
            if (y_base < 45 || y_base > 70) return;
            u8g2.drawRBox(74, y_base - 8, 24, 10, 2);
            u8g2.setDrawColor(0);
            u8g2.drawStr(76, y_base, "Temp");
            u8g2.setDrawColor(1);
            char buf[16];
            if (celsius) snprintf(buf, sizeof(buf), "%.1f", temp);
            else         snprintf(buf, sizeof(buf), "%.1f", temp + 273.15f);
            int tw = u8g2.getStrWidth(buf);
            u8g2.drawStr(124 - tw, y_base - 4, buf);
            u8g2.drawStr(114, y_base + 3, celsius ? "C" : "K");
        };

        int cy = 58 + anim_temp_y;
        int ny = 58 + anim_temp_y - PAGE_HEIGHT;
        drawPage(currentTempUnit, cy);
        drawPage(targetTempUnit,  ny);
    }

    void drawMainIdle() {
        if (!first_time) {
            m_ui.animate(anim_bg,        128,                    400, EasingType::EASE_OUT_CUBIC, PROTECTION::PROTECTED);
            m_ui.animate(anim_status_x,   45,                    450, EasingType::EASE_OUT_CUBIC, PROTECTION::PROTECTED);
            m_ui.animate(anim_mode_box, g_autoMode ? 22 : 34,    350, EasingType::EASE_OUT_CUBIC, PROTECTION::PROTECTED);
            loadState  = CharlesLoadState::BRACE_LOADING;
            first_time = true;
        }

        if (loadState == CharlesLoadState::BRACE_LOADING) {
            volumeBrace.onLoad();
            tempBrace.onLoad();
            loadState = CharlesLoadState::DONE;
        }

        if (vol_animating && anim_vol_y >= 17) {
            currentVolPage = targetVolPage;
            anim_vol_y     = 0;
            vol_animating  = false;
        }
        if (temp_animating && anim_temp_y >= 17) {
            currentTempUnit = targetTempUnit;
            g_tempIsK       = !currentTempUnit;
            anim_temp_y     = 0;
            temp_animating  = false;
        }

        U8G2& u8g2 = m_ui.getU8G2();
        u8g2.setDrawColor(1);

        u8g2.setClipWindow(0, 0, anim_bg, 10);
        u8g2.drawHLine(0, 9, 128);
        u8g2.setMaxClipWindow();

        u8g2.setFont(u8g2_font_5x7_tr);
        u8g2.drawStr(5, 8, "GAS Monitor");

        u8g2.setFont(u8g2_font_profont17_tr);
        if (!g_unitIsKpa) snprintf(m_buf, sizeof(m_buf), "%.1f hPa", pressure);
        else              snprintf(m_buf, sizeof(m_buf), "%.2f kPa", pressure / 10.0f);
        u8g2.drawStr(3, 28, m_buf);

        u8g2.setFont(u8g2_font_5x7_tr);
        u8g2.drawStr(5, 40, g_autoMode ? "AUTO" : "MANUAL");

        u8g2.setDrawColor(2);
        u8g2.drawBox(3, 32, anim_mode_box, 10);
        u8g2.setDrawColor(1);

        u8g2.setClipWindow(45, 34, 95, 42);
        u8g2.drawStr(anim_status_x, 40, "STANDBY");
        u8g2.setMaxClipWindow();

        u8g2.drawStr(100, 40, "NEXT");
        nextButton.draw();

        volumeBrace.draw();
        tempBrace.draw();
    }

    void enterSettings() {
        state = CharlesState::NEXT_PAGE;
        m_ui.clearFocusManager();

        m_settings.onStartExperiment = [this]() {
            charles.reset();
            charles.setStepMode(g_autoMode ? CharlesLaw::AUTO : CharlesLaw::MANUAL);
            charles.setTotalTargetVolume((float)g_targetVolume);
            if (!g_autoMode) {
                charles.clearTempSteps();
                for (int32_t i = 0; i < g_stepCount; i++)
                    charles.addTempStep((float)g_manualSteps[i]);
            }
            charles.start();

            state = CharlesState::RUNNING;
            m_ui.clearFocusManager();
            m_running.onReport = [this]() {
                enterReport(false);
            };
            m_running.enter([this]() {
                state = CharlesState::NEXT_PAGE;
                m_ui.setContinousDraw(true);
                m_settings.reattachFocus();
                m_ui.markDirty();
            });
        };

        m_settings.onOpenReport = [this]() {
            enterReport(true);
        };

        m_settings.onEnter([this]() {
            state         = CharlesState::MAIN_IDLE;
            first_time    = false;
            anim_bg       = 0;
            anim_mode_box = 0;
            anim_status_x = 10;
            m_ui.setContinousDraw(true);
            m_ui.addWidgetToFocusManager(&volumeBrace);
            m_ui.addWidgetToFocusManager(&tempBrace);
            m_ui.addWidgetToFocusManager(&nextButton);
            m_ui.markDirty();
        });
    }

    void enterReport(bool fromSettings) {
        m_ui.clearFocusManager();
        if (!fromSettings) m_report.saveSnapshot();
        m_reportFromSettings = fromSettings;
        m_report.enter([this]() {
            m_slidingIn = false;
            m_slideX    = 0;
            m_ui.animate(m_slideX, 128, 280, EasingType::EASE_IN_CUBIC, PROTECTION::PROTECTED);
        });
        m_slidingIn = true;
        m_slideX    = 128;
        state       = CharlesState::REPORT;
        m_ui.animate(m_slideX, 0, 280, EasingType::EASE_OUT_CUBIC, PROTECTION::PROTECTED);
    }

public:
    AppCharles(PixelUI& ui, void*) :
        m_ui(ui),
        state(CharlesState::MAIN_IDLE),
        loadState(CharlesLoadState::INIT),
        m_settings(ui),
        m_running(ui),
        m_report(ui),
        volumeBrace(ui, 3,  45, 56, 18),
        tempBrace  (ui, 69, 45, 56, 18),
        nextButton (ui, 89, 33,  8,  7, next_arrow_bits)
    {
        pressure = bme.getPressure();
    }

    void onEnter(ExitCallback cb) override {
        IApplication::onEnter(cb);
        m_ui.setContinousDraw(true);
        m_ui.markDirty();

        volumeBrace.setDrawContentFunction([this]() { volumeBraceContent(); });
        volumeBrace.setCallback([this]() {
            if (vol_animating) return;
            targetVolPage = (currentVolPage == VolumePage::VOL_A) ? VolumePage::VOL_L : VolumePage::VOL_A;
            anim_vol_y    = 0;
            m_ui.animate(anim_vol_y, 18, 300, EasingType::EASE_IN_OUT_CUBIC, PROTECTION::PROTECTED);
            vol_animating = true;
        });

        tempBrace.setDrawContentFunction([this]() { tempBraceContent(); });
        tempBrace.setCallback([this]() {
            if (temp_animating) return;
            targetTempUnit = !currentTempUnit;
            anim_temp_y    = 0;
            m_ui.animate(anim_temp_y, 18, 300, EasingType::EASE_IN_OUT_CUBIC, PROTECTION::PROTECTED);
            temp_animating = true;
        });

        nextButton.setCallback([this]() { enterSettings(); });

        m_ui.addWidgetToFocusManager(&volumeBrace);
        m_ui.addWidgetToFocusManager(&tempBrace);
        m_ui.addWidgetToFocusManager(&nextButton);

        state         = CharlesState::MAIN_IDLE;
        first_time    = false;
        anim_bg       = 0;
        anim_mode_box = 0;
        anim_status_x = 10;
    }

    void draw() override {
        pressure = bme.getPressure();

        if (state == CharlesState::MAIN_IDLE) { drawMainIdle(); return; }
        if (state == CharlesState::NEXT_PAGE) { m_settings.draw(); return; }

        if (state == CharlesState::REPORT) {
            if (m_reportFromSettings) m_settings.draw();
            else                      m_running.draw();

            if (m_slideX < 128) m_report.drawAt(m_slideX);

            if (!m_slidingIn && m_slideX >= 127) {
                m_slideX = 0;
                if (m_reportFromSettings) {
                    state = CharlesState::NEXT_PAGE;
                } else {
                    state = CharlesState::RUNNING;
                    m_ui.clearFocusManager();
                    m_running.reattachFocus();
                }
            }
            return;
        }

        m_running.draw();
    }

    bool handleInput(InputEvent event) override {
        if (state == CharlesState::MAIN_IDLE) {
            if (event == InputEvent::BACK) requestExit();
            return true;
        }
        if (state == CharlesState::NEXT_PAGE) return m_settings.handleInput(event);
        if (state == CharlesState::REPORT) {
            if (m_slideX > 0) return true;
            return m_report.handleInput(event);
        }
        return m_running.handleInput(event);
    }

    void onExit() override {
        if (state == CharlesState::RUNNING) charles.stop();
        m_ui.clearAllAnimations();
        m_ui.setContinousDraw(false);
    }
};

AppItem charles_app{
    .title  = "Charles' Law",
    .bitmap = image_charles_bits,
    .createApp = [](PixelUI& ui, void* parameter) -> std::unique_ptr<IApplication> {
        return std::unique_ptr<IApplication>(new AppCharles(ui, parameter));
    },
};
