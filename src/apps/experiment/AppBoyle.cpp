#include "AppBoyle.h"
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

static const unsigned char image_boyle_bits[] = {
    0xF0,0xFF,0x0F,0xFC,0xFF,0x3F,0xFE,0x00,0x7F,0xBE,0x7E,0x7E,
    0xBF,0xFE,0xFC,0x0F,0xF8,0xF9,0x0F,0xF8,0xFB,0x1F,0xFC,0xE0,
    0xEF,0x7B,0xDF,0xF7,0xB7,0xAF,0xFB,0xAF,0xB7,0xFB,0xAF,0xBB,
    0xFB,0xAF,0xBB,0x03,0x60,0xDF,0x83,0xE0,0xE0,0x43,0xE1,0xFF,
    0xA3,0x62,0xF8,0x83,0x60,0xFB,0x83,0x60,0xF8,0x83,0x60,0xF7,
    0x02,0x60,0x63,0x06,0x70,0x41,0xFC,0xFF,0x3F,0xF0,0xFF,0x0F
};

static const unsigned char next_arrow_bits[] = {
    0x10, 0x30, 0x70, 0xF0, 0x70, 0x30, 0x10
};

// Running page status icons (7x7)
static const unsigned char pause_icon_bits[] = { 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36 };
static const unsigned char play_icon_bits[]  = { 0x01, 0x03, 0x07, 0x0F, 0x07, 0x03, 0x01 };

// Experiment config
static bool    g_autoMode   = true;
static bool    g_unitIsKpa  = false;
static bool    g_tempIsK    = false;  // false=°C, true=K (synced with main page brace)
static bool    g_tempCtrl   = false;
static bool    g_valveOpen  = false;
static int32_t g_targetTemp = 25;   // °C

// MANUAL mode step globals
static int32_t g_manualSteps[BoyleLaw::MAX_STEPS] = {};
static int32_t g_stepCount = 0;

// ===== MANUAL Step sub-menu =====
class AppBoyleStepMenu : public ListView {
private:
    // title + 7 steps + 添加节点 + 删除末尾 + 清空全部 = 11 max
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
        setTitle(m_items[0], ">>> 体积节点 <<<");

        for (int32_t i = 0; i < g_stepCount; i++) {
            clearItem(m_items[i + 1]);
            char buf[12];
            snprintf(buf, sizeof(buf), "节点%d", (int)(i + 1));
            setTitle(m_items[i + 1], buf);
            m_items[i + 1].extra.intValue = &g_manualSteps[i];
            int32_t idx = i;
            m_items[i + 1].pFunc = [this, idx]() {
                m_ui.showPopupProgress(g_manualSteps[idx], 200, 450,
                    "体积(mL)", 100, 40, 5000, 1, nullptr, true, 25);
            };
        }

        int32_t base = g_stepCount + 1;

        clearItem(m_items[base]);
        if (g_stepCount < (int32_t)BoyleLaw::MAX_STEPS) {
            setTitle(m_items[base], "添加节点");
            m_items[base].pFunc = [this]() {
                if (g_stepCount >= (int32_t)BoyleLaw::MAX_STEPS) return;
                g_manualSteps[g_stepCount] = 300;
                g_stepCount++;
                rebuildItems();
                setCursor(g_stepCount + 1); // new "添加节点" position
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
            setCursor(g_stepCount + 2); // "删除末尾" at base+1 = (g_stepCount+1)+1
        };

        clearItem(m_items[base + 2]);
        setTitle(m_items[base + 2], "清空全部");
        m_items[base + 2].pFunc = [this]() {
            g_stepCount = 0;
            rebuildItems();
            setCursor(3); // "清空全部" is at index 3 when stepCount=0
        };

        resizeLength(g_stepCount + 3); // last valid index = stepCount + 3
    }

public:
    AppBoyleStepMenu(PixelUI& ui) : ListView(ui, m_items, 11) {}

    void onEnter(ExitCallback cb) override {
        rebuildItems();
        ListView::onEnter(cb);
    }

    void onLoad() override {}
    void onSave() override {}
    void onExit() override {}
};

// ===== Settings ListView =====
class AppBoyleSettings : public ListView {
private:
    // 8 items: title + 4 common + 体积节点 + valve + start + report
    ListItem         m_items[9];
    AppBoyleStepMenu m_stepMenu;
    bool             m_inStepMenu = false;

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
        if (g_autoMode) {
            clearItem(m_items[5]);
            setTitle(m_items[5], "体积节点");
            m_items[5].pFunc = [this]() {
                m_ui.showPopupInfo(
                    "400>375>350>325\n300>275>250 mL",
                    "AUTO 节点", 110, 40, 3000);
            };

            clearItem(m_items[6]);
            setTitle(m_items[6], "气阀");
            m_items[6].extra.switchValue = &g_valveOpen;
            m_items[6].pFunc = [this]() {
                if (g_valveOpen) boyle.openValve();
                else             boyle.closeValve();
            };

            clearItem(m_items[7]);
            setTitle(m_items[7], "开始实验");
            m_items[7].pFunc = [this]() {
                if (onStartExperiment) onStartExperiment();
            };

            clearItem(m_items[8]);
            setTitle(m_items[8], "实验报告");
            m_items[8].pFunc = [this]() {
                if (onOpenReport) onOpenReport();
            };

            resizeLength(8);
        } else {
            clearItem(m_items[5]);
            setTitle(m_items[5], "体积节点");
            m_items[5].extra.intValue = &g_stepCount;
            m_items[5].pFunc = [this]() {
                m_inStepMenu = true;
                m_stepMenu.onEnter([this]() {
                    m_inStepMenu = false;
                });
            };

            clearItem(m_items[6]);
            setTitle(m_items[6], "气阀");
            m_items[6].extra.switchValue = &g_valveOpen;
            m_items[6].pFunc = [this]() {
                if (g_valveOpen) boyle.openValve();
                else             boyle.closeValve();
            };

            clearItem(m_items[7]);
            setTitle(m_items[7], "开始实验");
            m_items[7].pFunc = [this]() {
                if (onStartExperiment) onStartExperiment();
            };

            clearItem(m_items[8]);
            setTitle(m_items[8], "实验报告");
            m_items[8].pFunc = [this]() {
                if (onOpenReport) onOpenReport();
            };

            resizeLength(8);
        }
    }

    void setupItems() {
        setTitle(m_items[0], ">>> 实验设置 <<<");

        setTitle(m_items[1], "自动模式");
        m_items[1].extra.switchValue = &g_autoMode;
        m_items[1].pFunc = [this]() { rebuildModeItems(); };

        setTitle(m_items[2], "压强单位 kPa");
        m_items[2].extra.switchValue = &g_unitIsKpa;

        setTitle(m_items[3], "温度控制");
        m_items[3].extra.switchValue = &g_tempCtrl;

        setTitle(m_items[4], "目标温度");
        m_items[4].extra.intValue = &g_targetTemp;
        m_items[4].pFunc = [this]() {
            if (!g_tempCtrl) return;
            m_ui.showPopupProgress(g_targetTemp, 20, 40, "目标温度(C)",
                                   100, 40, 4000, 1, nullptr, true, 1);
        };

        rebuildModeItems();
    }

public:
    std::function<void()> onStartExperiment;
    std::function<void()> onOpenReport;

    AppBoyleSettings(PixelUI& ui) : ListView(ui, m_items, 9), m_stepMenu(ui) {
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

    void onLoad() override { g_valveOpen = boyle.isValveOpen(); }
    void onSave() override {}
    void onExit() override {}
};

// ===== Running page =====
class AppBoyleRunning {
private:
    PixelUI&   m_ui;
    Brace      m_brace;
    CurveChart m_histogram;
    Blinker    m_blinker;
    Coroutine  m_entryCoro;
    IconButton m_reportBtn;

    bool     m_lastDataCount    = 0;
    uint32_t m_prevTime         = 0;
    char     m_buf[20]          = {};

    // Entry animation variables
    int32_t anim_bg       = 0;    // divider line left-to-right
    int32_t anim_temp_x   = -30;  // temperature text slide in
    int32_t anim_mark_m   = 0;    // status block width
    int32_t anim_status_x = -27;  // description text slide in

    std::function<void()> m_onExit;

    // ---- Coroutine body ----
    void entryCoro(CoroutineContext& ctx) {
        CORO_BEGIN(ctx);
            m_ui.animate(anim_bg,     128, 400, EasingType::EASE_OUT_CUBIC, PROTECTION::PROTECTED);
            m_ui.animate(anim_temp_x,   0, 350, EasingType::EASE_OUT_CUBIC, PROTECTION::PROTECTED);
        CORO_DELAY(ctx, m_ui, 420, 10);
            m_brace.onLoad();
        CORO_DELAY(ctx, m_ui, 80, 20);
            m_ui.addWidgetToFocusManager(&m_brace);
            m_ui.addWidgetToFocusManager(&m_histogram);
            m_ui.addWidgetToFocusManager(&m_reportBtn);
            m_ui.animate(anim_mark_m,   23, 300, EasingType::EASE_OUT_QUAD,   PROTECTION::PROTECTED);
            m_ui.animate(anim_status_x, 38, 450, EasingType::EASE_OUT_CUBIC,  PROTECTION::PROTECTED);
        CORO_END(ctx);
    }

    // ---- State block helpers ----
    struct StateInfo {
        const char* tag;         // short label inside color block (≤4 chars)
        const char* desc;        // description text (right of block)
        uint32_t    blinkMs;     // 0 = no blink
    };

    StateInfo stateInfo() {
        switch (boyle.getState()) {
            case BoyleLaw::INIT_VOLUME:   return {"Fill", "INITVOL", 0};
            case BoyleLaw::TEMP_CHECKING: return {"TmpC", "CHEKTMP",  0};
            case BoyleLaw::WAITING_TEMP:  return {"TmpW", "WAITTMP", 500};
            case BoyleLaw::STEPPING:      return {"Vol",  "ADJVOL",  0};
            case BoyleLaw::CIRCULATING:   return {"Mix",  "MIXGAS",  0};
            case BoyleLaw::STABILIZING:   return {"Stab", "STABLE",  500};
            case BoyleLaw::RECORDING:     return {"Rec",  "RECORD",  150};
            case BoyleLaw::DONE:          return {"Done", "DONE",    0};
            case BoyleLaw::ERROR:         return {"Err",  "ERROR",   300};
            default:                      return {"Idle", "IDLE",    0};
        }
    }

    void drawMainArea() {
        U8G2& u8g2 = m_ui.getU8G2();

        // ---- Main pressure value (profont17, baseline y=30) ----
        u8g2.setFont(u8g2_font_profont17_tr);
        float p = bme.getPressure();
        if (isnan(p)) {
            if (!g_unitIsKpa) snprintf(m_buf, sizeof(m_buf), "---.- hPa");
            else              snprintf(m_buf, sizeof(m_buf), "---.- kPa");
        } else if (!g_unitIsKpa) {
            snprintf(m_buf, sizeof(m_buf), "%.1f hPa", p);
        } else {
            snprintf(m_buf, sizeof(m_buf), "%.2f kPa", p / 10.0f);
        }
        u8g2.drawStr(3, 30, m_buf);

        // ---- State block row (y=35..43) ----
        StateInfo si = stateInfo();

        // Update blinker interval and state
        if (si.blinkMs > 0) {
            m_blinker.set_interval(si.blinkMs);
            m_blinker.start();
        } else {
            m_blinker.stopOnVisible();
        }

        u8g2.setFont(u8g2_font_5x7_tr);

        // Always draw for non-blinking states; blinker controls blinking states
        bool shouldDraw = (si.blinkMs == 0) || m_blinker.is_visible();
        if (shouldDraw) {
            // Step 1: draw tag text first
            u8g2.setDrawColor(1);
            u8g2.drawStr(5, 42, si.tag);

            // Step 2: overlay inverted box to produce reverse-video effect
            u8g2.setDrawColor(2);
            u8g2.drawBox(3, 35, anim_mark_m, 8);
            u8g2.setDrawColor(1);

            // Step 3: description text, clipped to right of block
            int clip_x = anim_mark_m + 14;
            u8g2.setClipWindow(clip_x, 36, 128, 43);
            u8g2.drawStr(anim_status_x, 42, si.desc);
            u8g2.setMaxClipWindow();
        }

        // RPT button — always visible for testing
        u8g2.setFont(u8g2_font_5x7_tr);
        u8g2.drawStr(108, 42, "RPT");
        m_reportBtn.draw();
    }

    // ---- Brace page flip ----
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
        const int PH = 18; // page height

        // Settle animation
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
                u8g2.drawStr(10, yBase, "Vol");
                u8g2.setDrawColor(1);
                float v = volumeCtrl.getAirVolume();
                snprintf(m_buf, sizeof(m_buf), isnan(v) ? "--" : "%.0f", v);
                int tw = u8g2.getStrWidth(m_buf);
                u8g2.drawStr(58 - tw, yBase - 4, m_buf);
                u8g2.drawStr(50, yBase + 3, "mL");
            } else {
                u8g2.drawStr(10, yBase, "Next");
                u8g2.setDrawColor(1);

                // Before experiment starts: show first step; during run: show next step
                BoyleLaw::State s = boyle.getState();
                bool running = (s == BoyleLaw::STEPPING || s == BoyleLaw::CIRCULATING
                             || s == BoyleLaw::STABILIZING || s == BoyleLaw::RECORDING);
                float t = running ? boyle.getNextTargetVolume()
                                  : boyle.getCurrentTargetVolume();

                if (t > 0.0f) {
                    snprintf(m_buf, sizeof(m_buf), "%.0f", t);
                    int tw = u8g2.getStrWidth(m_buf);
                    u8g2.drawStr(58 - tw, yBase - 4, m_buf);
                } else {
                    u8g2.drawStr(36, yBase - 4, "End");
                }
                u8g2.drawStr(50, yBase + 3, "mL");
            }
        };

        int cy = 58 + m_braceAnimY;
        int ny = 58 + m_braceAnimY - PH;
        drawPage(m_bracePage,       cy);
        drawPage(m_bracePageTarget, ny);
    }

    // ---- Status bar ----
    void drawStatusBar() {
        U8G2& u8g2 = m_ui.getU8G2();
        u8g2.setFont(u8g2_font_5x7_tr);

        // Left: temperature slides in from left
        float t = bme.getTemperature();
        if (isnan(t))       snprintf(m_buf, sizeof(m_buf), "T=-- C");
        else if (g_tempIsK) snprintf(m_buf, sizeof(m_buf), "T=%.0fK",  t + 273.15f);
        else                snprintf(m_buf, sizeof(m_buf), "T=%.1fC",  t);
        u8g2.drawStr(anim_temp_x, 7, m_buf);

        // Center: step progress bar (x=38..110, 72px wide, y=1..7)
        uint8_t total = boyle.getTotalSteps();
        if (total > 0) {
            uint8_t cur   = boyle.getCurrentStep();
            // Step count text outside bar on the right
            snprintf(m_buf, sizeof(m_buf), "%d/%d", cur, total);
            int tw    = u8g2.getStrWidth(m_buf);
            int bar_x = 38;
            int bar_w = 110 - bar_x - tw - 3;  // leave room for text + 3px gap
            int bar_y = 2;
            int bar_h = 5;
            int filled = (int)((float)cur / total * bar_w);

            u8g2.drawFrame(bar_x, bar_y, bar_w, bar_h);
            if (filled > 0) u8g2.drawBox(bar_x, bar_y, filled, bar_h);

            u8g2.drawStr(bar_x + bar_w + 3, 7, m_buf);
        }

        // Right: running icon (static)
        u8g2.drawXBMP(119, 1, 7, 7, pause_icon_bits);

        // Divider: animate left-to-right via clip window
        u8g2.setClipWindow(0, 9, anim_bg, 10);
        u8g2.drawHLine(0, 9, 128);
        u8g2.setMaxClipWindow();
    }

public:
    std::function<void()> onReport;

    AppBoyleRunning(PixelUI& ui)
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
        m_onExit      = onExit;
        m_lastDataCount  = 0;
        m_prevTime       = m_ui.getCurrentTime();
        anim_bg          = 0;
        anim_temp_x   = -30;
        anim_mark_m   = 0;
        anim_status_x = -27;

        m_blinker.stopOnVisible();
        m_bracePage      = BracePage::CURRENT;
        m_bracePageTarget = BracePage::CURRENT;
        m_braceAnimY     = 0;
        m_braceAnimating = false;

        m_brace.setDrawContentFunction([this]() { braceContent(); });
        m_brace.setCallback([this]() { braceCallback(); });

        m_reportBtn.onLoad();
        m_reportBtn.setCallback([this]() {
            if (onReport) onReport();
        });

        // onLoad allocates the data buffer — must be called before addData
        m_histogram.onLoad();

        m_entryCoro.reset();
        m_entryCoro.start();
        m_ui.addCoroutine(&m_entryCoro);

        m_ui.setContinousDraw(true);
        m_ui.markDirty();
    }

    void draw() {
        m_blinker.update();

        // Data acquisition: only real-time BME pressure for smooth curve
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
            boyle.stop();
            boyle.reset();
            m_ui.removeCoroutine(&m_entryCoro);
            m_ui.clearFocusManager();
            if (m_onExit) m_onExit();
            return true;
        }
        // Delegate LEFT / RIGHT / SELECT to PixelUI focus manager
        m_ui.handleInput(event);
        return true;
    }
};

// ===== Main App =====
enum class BoyleState { MAIN_IDLE, NEXT_PAGE, RUNNING, REPORT };
enum class LoadState   { INIT, BRACE_LOADING, DONE };

// ===== Report page =====
class AppBoyleReport {
private:
    PixelUI& m_ui;
    int      m_scrollOffset = 0;   // first visible data row index
    int      m_scrollAnim   = 0;   // animated pixel offset for smooth scroll
    int      m_scrollTarget = 0;
    char     m_buf[20]      = {};

    std::function<void()> m_onExit;

    // 快照数据（实验结束/退出后持久保留）
    BoyleLaw::DataPoint m_snapData[BoyleLaw::MAX_STEPS] = {};
    uint8_t             m_snapCount    = 0;
    bool                m_snapUnitKpa  = false;
    bool                m_snapTempIsK  = false;

    static const int ROW_H    = 9;   // row height px
    static const int VISIBLE  = 5;   // visible data rows (y=9..53, 5×9=45px)
    static const int HEADER_Y = 8;   // table header baseline
    static const int DATA_Y0  = 17;  // first data row baseline
    static const int SCROLL_X = 125; // scrollbar x

    void drawRow(int rowIndex, int yBase) { drawRowAt(rowIndex, yBase, 0); }
    void drawRowAt(int rowIndex, int yBase, int xOff) {
        U8G2& u8g2 = m_ui.getU8G2();
        u8g2.setFont(u8g2_font_5x7_tr);

        if (rowIndex < (int)m_snapCount) {
            const auto& dp = m_snapData[rowIndex];

            snprintf(m_buf, sizeof(m_buf), "%d", rowIndex + 1);
            u8g2.drawStr(xOff + 0, yBase, m_buf);

            snprintf(m_buf, sizeof(m_buf), "%.0f", dp.V_total);
            int tw = u8g2.getStrWidth(m_buf);
            u8g2.drawStr(xOff + 38 - tw, yBase, m_buf);

            if (!m_snapUnitKpa) snprintf(m_buf, sizeof(m_buf), "%.1f", dp.P);
            else                snprintf(m_buf, sizeof(m_buf), "%.2f", dp.P / 10.0f);
            tw = u8g2.getStrWidth(m_buf);
            u8g2.drawStr(xOff + 83 - tw, yBase, m_buf);

            snprintf(m_buf, sizeof(m_buf), "%.1f", dp.PV / 1000.0f);
            tw = u8g2.getStrWidth(m_buf);
            u8g2.drawStr(xOff + 121 - tw, yBase, m_buf);
        }
    }

    void drawScrollbar(int total) { drawScrollbarAt(total, 0); }
    void drawScrollbarAt(int total, int xOff) {
        if (total <= VISIBLE) return;
        U8G2& u8g2 = m_ui.getU8G2();
        // ListView-style: single vertical line, short segment marks current position
        // Track: y=9..53 (44px, same as data area)
        const int TRACK_TOP = 9;
        const int TRACK_H   = 44;
        int seg_h   = std::max(4, TRACK_H * VISIBLE / total);
        int seg_y   = TRACK_TOP + (TRACK_H - seg_h) * m_scrollOffset / std::max(1, total - VISIBLE);
        u8g2.drawVLine(xOff + SCROLL_X, seg_y, seg_h);
    }

public:
    AppBoyleReport(PixelUI& ui) : m_ui(ui) {}

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
        // Smooth scroll animation
        if (m_scrollAnim != m_scrollTarget) {
            int diff = m_scrollTarget - m_scrollAnim;
            m_scrollAnim += (diff > 0 ? 1 : -1) * std::max(1, abs(diff) / 2 + 1);
            if (abs(m_scrollTarget - m_scrollAnim) <= 1) m_scrollAnim = m_scrollTarget;
        }

        U8G2& u8g2 = m_ui.getU8G2();

        // Fill background to cover running page beneath
        u8g2.setDrawColor(0);
        u8g2.drawBox(xOff, 0, 128, 64);
        u8g2.setDrawColor(1);

        u8g2.setFont(u8g2_font_5x7_tr);

        // Header
        u8g2.drawStr(xOff + 0,  HEADER_Y, "#");
        u8g2.drawStr(xOff + 18, HEADER_Y, "Vol");
        const char* pUnit = m_snapUnitKpa ? "kPa" : "hPa";
        u8g2.drawStr(xOff + 58, HEADER_Y, pUnit);
        u8g2.drawStr(xOff + 95, HEADER_Y, "PV/k");
        u8g2.drawHLine(xOff, 9, 123);

        int cnt = (int)m_snapCount;

        if (cnt == 0) {
            u8g2.drawStr(xOff + 30, 35, "No Data");
        } else {
            // Data rows with pixel-level scroll — clip above bottom bar (y=54)
            u8g2.setClipWindow(xOff, 10, xOff + 123, 53);
            for (int i = 0; i < VISIBLE + 1; i++) {
                int rowIndex = m_scrollOffset + i;
                int yBase    = DATA_Y0 + i * ROW_H - (m_scrollAnim % ROW_H);
                if (yBase > 63) break;
                drawRowAt(rowIndex, yBase, xOff);
            }
            u8g2.setMaxClipWindow();
        }

        // Separator + scrollbar
        drawScrollbarAt(cnt, xOff);

        // Bottom bar: AvgT | MaxErr | AvgErr
        u8g2.drawHLine(xOff, 54, 123);
        if (cnt > 0) {
            // Avg temperature
            float tSum = 0;
            for (int i = 0; i < cnt; i++) tSum += m_snapData[i].T;
            float avgT = tSum / cnt;
            if (m_snapTempIsK)
                snprintf(m_buf, sizeof(m_buf), "T:%.0fK", avgT);
            else
                snprintf(m_buf, sizeof(m_buf), "T:%.1fC", avgT - 273.15f);
            u8g2.drawStr(xOff, 63, m_buf);

            // MaxErr and AvgErr relative to first data point PV
            if (cnt > 1) {
                float pvRef  = m_snapData[0].PV;
                float errSum = 0, maxErr = 0;
                for (int i = 1; i < cnt; i++) {
                    float e = fabsf(m_snapData[i].PV - pvRef) / pvRef * 100.0f;
                    if (e > maxErr) maxErr = e;
                    errSum += e;
                }
                float avgErr = errSum / (cnt - 1);
                snprintf(m_buf, sizeof(m_buf), "Max:%.2f%%", maxErr);
                u8g2.drawStr(xOff + 36, 63, m_buf);
                snprintf(m_buf, sizeof(m_buf), "Avg:%.2f%%", avgErr);
                u8g2.drawStr(xOff + 80, 63, m_buf);
            } else {
                u8g2.drawStr(xOff + 36, 63, "Max:---");
                u8g2.drawStr(xOff + 80, 63, "Avg:---");
            }
        } else {
            u8g2.drawStr(xOff     , 63, "T:--- ");
            u8g2.drawStr(xOff + 36, 63, "Max:---");
            u8g2.drawStr(xOff + 80, 63, "Avg:---");
        }
    }

    bool handleInput(InputEvent event) {
        int cnt = (int)m_snapCount;
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

    void saveSnapshot() {
        m_snapCount   = boyle.getDataCount();
        m_snapUnitKpa = g_unitIsKpa;
        m_snapTempIsK = g_tempIsK;
        for (uint8_t i = 0; i < m_snapCount; i++)
            m_snapData[i] = boyle.getDataPoint(i);
    }
    bool hasSnapshot() const { return m_snapCount > 0; }
};

class AppBoyle : public IApplication {
private:
    PixelUI&         m_ui;
    BoyleState       state;
    LoadState        loadState;
    AppBoyleSettings m_settings;
    AppBoyleRunning  m_running;
    AppBoyleReport   m_report;

    int32_t m_slideX      = 0;
    bool    m_slidingIn   = false;
    bool    m_reportFromSettings = false; // true = entered from settings, false = from running

    Brace      tempBrace;
    Brace      volumeBrace;
    IconButton nextButton;

    bool    first_time    = false;
    int32_t anim_status_x = 10;
    int32_t anim_mode_box = 0;
    int32_t anim_bg       = 0;
    float   pressure      = 0.0f;

    bool    currentTempUnit = true;
    bool    targetTempUnit  = true;
    int32_t anim_temp_y     = 0;
    bool    temp_animating  = false;

    enum class VolumePage { VOL_A = 0, VOL_L = 1 };
    VolumePage currentVolPage = VolumePage::VOL_A;
    VolumePage targetVolPage  = VolumePage::VOL_A;
    int32_t    anim_vol_y     = 0;
    bool       vol_animating  = false;

    void tempBraceContent() {
        U8G2& u8g2 = m_ui.getU8G2();
        u8g2.setFont(u8g2_font_5x7_tr);
        const int PAGE_HEIGHT = 18;
        float temp = bme.getTemperature();

        auto drawPage = [&](bool celsius, int y_base) {
            u8g2.drawRBox(8, y_base - 8, 24, 10, 2);
            u8g2.setDrawColor(0);
            u8g2.drawStr(10, y_base, "Temp");
            u8g2.setDrawColor(1);
            char buf[16];
            if (celsius) snprintf(buf, sizeof(buf), "%.1f", temp);
            else         snprintf(buf, sizeof(buf), "%.1f", temp + 273.15f);
            int tw = u8g2.getStrWidth(buf);
            u8g2.drawStr(58 - tw, y_base - 4, buf);
            u8g2.drawStr(50, y_base + 3, celsius ? "C" : "K");
        };

        int cy = 58 + anim_temp_y;
        int ny = 58 + anim_temp_y - PAGE_HEIGHT;
        if (cy >= 45 && cy <= 70) drawPage(currentTempUnit, cy);
        if (ny >= 45 && ny <= 70) drawPage(targetTempUnit, ny);
    }

    void volumeBraceContent() {
        U8G2& u8g2 = m_ui.getU8G2();
        u8g2.setFont(u8g2_font_5x7_tr);
        const int PAGE_HEIGHT = 18;
        float volAir = volumeCtrl.getAirVolume();
        float volLiq = volumeCtrl.getLiquidVolume();

        auto drawPage = [&](VolumePage page, int y_base) {
            const char* label = (page == VolumePage::VOL_A) ? "VolA" : "VolL";
            float value       = (page == VolumePage::VOL_A) ? volAir : volLiq;
            u8g2.drawRBox(74, y_base - 8, 24, 10, 2);
            u8g2.setDrawColor(0);
            u8g2.drawStr(76, y_base, label);
            u8g2.setDrawColor(1);
            char buf[16];
            snprintf(buf, sizeof(buf), "%.0f", value);
            int tw = u8g2.getStrWidth(buf);
            u8g2.drawStr(124 - tw, y_base - 4, buf);
            u8g2.drawStr(114, y_base + 3, "mL");
        };

        int cy = 58 + anim_vol_y;
        int ny = 58 + anim_vol_y - PAGE_HEIGHT;
        if (cy >= 45 && cy <= 70) drawPage(currentVolPage, cy);
        if (ny >= 45 && ny <= 70) drawPage(targetVolPage, ny);
    }

    void drawMainIdle() {
        if (!first_time) {
            m_ui.animate(anim_bg,        128,                    400, EasingType::EASE_OUT_CUBIC, PROTECTION::PROTECTED);
            m_ui.animate(anim_status_x,   45,                    450, EasingType::EASE_OUT_CUBIC, PROTECTION::PROTECTED);
            m_ui.animate(anim_mode_box, g_autoMode ? 22 : 34,    350, EasingType::EASE_OUT_CUBIC, PROTECTION::PROTECTED);
            loadState  = LoadState::BRACE_LOADING;
            first_time = true;
        }

        if (loadState == LoadState::BRACE_LOADING) {
            tempBrace.onLoad();
            volumeBrace.onLoad();
            loadState = LoadState::DONE;
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
        char buf[32];
        if (!g_unitIsKpa) snprintf(buf, sizeof(buf), "%.1f hPa", pressure);
        else              snprintf(buf, sizeof(buf), "%.2f kPa", pressure / 10.0f);
        u8g2.drawStr(3, 28, buf);

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

        tempBrace.draw();
        volumeBrace.draw();
    }

    void enterSettings() {
        state = BoyleState::NEXT_PAGE;
        m_ui.clearFocusManager();

        m_settings.onStartExperiment = [this]() {
            // Configure and start BoyleLaw
            boyle.reset();
            boyle.setStepMode(g_autoMode ? BoyleLaw::AUTO : BoyleLaw::MANUAL);
            boyle.setTargetTemp((float)g_targetTemp);
            boyle.enableTempControl(g_tempCtrl);
            if (!g_autoMode) {
                boyle.clearVolumeSteps();
                for (int32_t i = 0; i < g_stepCount; i++)
                    boyle.addVolumeStep((float)g_manualSteps[i]);
            }
            boyle.start();

            state = BoyleState::RUNNING;
            m_ui.clearFocusManager();
            m_running.onReport = [this]() {
                m_ui.clearFocusManager();
                m_report.saveSnapshot();
                m_report.enter([this]() {
                    m_slidingIn = false;
                    m_slideX    = 0;
                    m_ui.animate(m_slideX, 128, 280, EasingType::EASE_IN_CUBIC, PROTECTION::PROTECTED);
                });
                m_reportFromSettings = false;
                m_slidingIn = true;
                m_slideX    = 128;
                state       = BoyleState::REPORT;
                m_ui.animate(m_slideX, 0, 280, EasingType::EASE_OUT_CUBIC, PROTECTION::PROTECTED);
            };
            m_running.enter([this]() {
                state = BoyleState::NEXT_PAGE;
                m_ui.setContinousDraw(true);
                m_ui.markDirty();
            });
        };

        m_settings.onOpenReport = [this]() {
            m_ui.clearFocusManager();
            m_report.enter([this]() {
                m_slidingIn          = false;
                m_slideX             = 0;
                m_ui.animate(m_slideX, 128, 280, EasingType::EASE_IN_CUBIC, PROTECTION::PROTECTED);
            });
            m_reportFromSettings = true;
            m_slidingIn = true;
            m_slideX    = 128;
            state       = BoyleState::REPORT;
            m_ui.animate(m_slideX, 0, 280, EasingType::EASE_OUT_CUBIC, PROTECTION::PROTECTED);
        };

        m_settings.onEnter([this]() {
            state         = BoyleState::MAIN_IDLE;
            first_time    = false;
            anim_bg       = 0;
            anim_mode_box = 0;
            anim_status_x = 10;
            m_ui.setContinousDraw(true);
            m_ui.addWidgetToFocusManager(&tempBrace);
            m_ui.addWidgetToFocusManager(&volumeBrace);
            m_ui.addWidgetToFocusManager(&nextButton);
            m_ui.markDirty();
        });
    }

public:
    AppBoyle(PixelUI& ui, void* parameter) :
        m_ui(ui),
        state(BoyleState::MAIN_IDLE),
        loadState(LoadState::INIT),
        m_settings(ui),
        m_running(ui),
        m_report(ui),
        tempBrace(ui, 3, 45, 56, 18),
        volumeBrace(ui, 69, 45, 56, 18),
        nextButton(ui, 89, 33, 8, 7, next_arrow_bits)
    {
        pressure = bme.getPressure();
    }

    void onEnter(ExitCallback cb) override {
        IApplication::onEnter(cb);
        m_ui.setContinousDraw(true);
        m_ui.markDirty();

        tempBrace.setDrawContentFunction([this]() { tempBraceContent(); });
        tempBrace.setCallback([this]() {
            if (temp_animating) return;
            targetTempUnit = !currentTempUnit;
            anim_temp_y    = 0;
            m_ui.animate(anim_temp_y, 18, 300, EasingType::EASE_IN_OUT_CUBIC, PROTECTION::PROTECTED);
            temp_animating = true;
        });

        volumeBrace.setDrawContentFunction([this]() { volumeBraceContent(); });
        volumeBrace.setCallback([this]() {
            if (vol_animating) return;
            targetVolPage = (currentVolPage == VolumePage::VOL_A) ? VolumePage::VOL_L : VolumePage::VOL_A;
            anim_vol_y    = 0;
            m_ui.animate(anim_vol_y, 18, 300, EasingType::EASE_IN_OUT_CUBIC, PROTECTION::PROTECTED);
            vol_animating = true;
        });

        nextButton.setCallback([this]() { enterSettings(); });

        m_ui.addWidgetToFocusManager(&tempBrace);
        m_ui.addWidgetToFocusManager(&volumeBrace);
        m_ui.addWidgetToFocusManager(&nextButton);

        state         = BoyleState::MAIN_IDLE;
        first_time    = false;
        anim_bg       = 0;
        anim_mode_box = 0;
        anim_status_x = 10;
    }

    void draw() override {
        pressure = bme.getPressure();

        if (state == BoyleState::MAIN_IDLE) { drawMainIdle(); return; }
        if (state == BoyleState::NEXT_PAGE) { m_settings.draw(); return; }

        if (state == BoyleState::REPORT) {
            // Draw background page underneath
            if (m_reportFromSettings) m_settings.draw();
            else                      m_running.draw();

            // Report page slides in/out on top
            if (m_slideX < 128) {
                m_report.drawAt(m_slideX);
            }

            // Exit animation finished — return to origin page
            if (!m_slidingIn && m_slideX >= 127) {
                m_slideX = 0;
                if (m_reportFromSettings) {
                    state = BoyleState::NEXT_PAGE;
                } else {
                    state = BoyleState::RUNNING;
                    m_ui.clearFocusManager();
                    m_running.reattachFocus();
                }
            }
            return;
        }

        m_running.draw();
    }

    bool handleInput(InputEvent event) override {
        if (state == BoyleState::MAIN_IDLE) {
            if (event == InputEvent::BACK) requestExit();
            return true;
        }
        if (state == BoyleState::NEXT_PAGE) return m_settings.handleInput(event);
        if (state == BoyleState::REPORT) {
            // Swallow all input during slide animation
            if (m_slideX > 0) return true;
            return m_report.handleInput(event);
        }
        return m_running.handleInput(event);
    }

    void onExit() override {
        if (state == BoyleState::RUNNING) boyle.stop();
        m_ui.clearAllAnimations();
        m_ui.setContinousDraw(false);
    }
};

AppItem boyle_app{
    .title  = "Boyle's Law",
    .bitmap = image_boyle_bits,
    .createApp = [](PixelUI& ui, void* parameter) -> std::unique_ptr<IApplication> {
        return std::unique_ptr<IApplication>(new AppBoyle(ui, parameter));
    },
};
