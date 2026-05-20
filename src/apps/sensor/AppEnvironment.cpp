#include <core/app/IApplication.h>
#include <core/app/app_system.h>
#include <focus/focus.h>
#include <widgets/curve_chart/curve_chart.h>
#include "HardwareManager.h"
#include "AppEnvironment.h"

__attribute__((aligned(4)))
static const unsigned char image_environment_bits[] = {0xf0,0xff,0x0f,0xfc,0xff,0x3f,0xfe,0xff,0x7f,0xce,0xe7,0x7f,0xb7,0xdb,0xff,0xb7,0xbd,0xff,0x7b,0xb5,0xff,0xfd,0xa4,0xff,0xfd,0x34,0xf7,0xfe,0x25,0xe9,0xfe,0xb5,0xee,0xff,0xa5,0xdf,0xff,0xb5,0xdf,0xff,0x76,0xbf,0x7f,0xe7,0x7d,0x7f,0xdb,0xfa,0x7f,0x5b,0xf7,0x7f,0xa7,0xef,0xff,0xbe,0xee,0xff,0xbd,0xed,0xfe,0x43,0x77,0xfe,0xff,0x78,0xfc,0xff,0x3f,0xf0,0xff,0x0f};
__attribute__((aligned(4)))
static const uint8_t image_barometer_bits[] = {0xf0,0x01,0x08,0x02,0x04,0x04,0x02,0x09,0x81,0x10,0x81,0x10,0x41,0x10,0x41,0x10,0x21,0x10,0x21,0x10,0x30,0x00,0x10,0x00};
__attribute__((aligned(4)))
static const uint8_t image_volume_bits[] = {0xf0,0x0f,0x08,0x10,0x08,0x10,0x08,0x10,0x10,0x08,0x0c,0x30,0x04,0x20,0x04,0x20,0x04,0x20,0x04,0x20,0xc4,0x2f,0xf4,0x2c,0x94,0x2f,0xe4,0x27,0x08,0x10,0xf0,0x0f};
__attribute__((aligned(4)))
static const uint8_t image_temperature_bits[] = {0x38,0x00,0x44,0x40,0xd4,0xa0,0x54,0x40,0xd4,0x1c,0x54,0x06,0xd4,0x02,0x54,0x02,0x54,0x06,0x92,0x1c,0x39,0x01,0x75,0x01,0x7d,0x01,0x39,0x01,0x82,0x00,0x7c,0x00};

class AppEnvironment : public IApplication {
private:
    PixelUI& m_ui;
    FocusManager focusMan;
    // 顺序：压强（上）、温度（中）、容积（下）
    CurveChart chart_baro;
    CurveChart chart_temp;
    CurveChart chart_vol;

    uint32_t timestamp_prev = 0;
    int32_t anim_pressure_x = -45;  // 第一行（压强）
    int32_t anim_temp_x     = -45;  // 第二行（温度）
    int32_t anim_vol_x      = -45;  // 第三行（容积）

    Coroutine coroutine_load;
    char print_buffer[15];

    void animation_coroutine_body(CoroutineContext& ctx) {
        CORO_BEGIN(ctx);
        CORO_DELAY(ctx, m_ui, 50, 1);
        chart_baro.onLoad();
        m_ui.animate(anim_pressure_x, 0, 320, EasingType::EASE_OUT_QUAD, PROTECTION::PROTECTED);
        CORO_DELAY(ctx, m_ui, 50, 2);
        chart_temp.onLoad();
        m_ui.animate(anim_temp_x, 0, 320, EasingType::EASE_OUT_QUAD, PROTECTION::PROTECTED);
        CORO_DELAY(ctx, m_ui, 50, 3);
        chart_vol.onLoad();
        m_ui.animate(anim_vol_x, 0, 320, EasingType::EASE_OUT_QUAD, PROTECTION::PROTECTED);
        CORO_END(ctx);
    }

public:
    AppEnvironment(PixelUI& ui)
        : m_ui(ui),
          focusMan(m_ui),
          chart_baro(m_ui, 69,  2, 56, 19, 110, 19, EXPAND_BASE::BOTTOM_RIGHT),
          chart_temp(m_ui, 69, 23, 56, 19, 110, 19, EXPAND_BASE::BOTTOM_RIGHT),
          chart_vol (m_ui, 69, 44, 56, 19, 110, 19, EXPAND_BASE::BOTTOM_RIGHT),
          coroutine_load([this](CoroutineContext& ctx) {
              animation_coroutine_body(ctx);
          })
    {}

    void onEnter(ExitCallback cb) override {
        IApplication::onEnter(cb);
        m_ui.setContinousDraw(true);
        timestamp_prev = m_ui.getCurrentTime();

        focusMan.addWidget(&chart_baro);
        focusMan.addWidget(&chart_temp);
        focusMan.addWidget(&chart_vol);

        coroutine_load.start();
        m_ui.addCoroutine(&coroutine_load);
    }

    void draw() override {
        U8G2& u8g2 = m_ui.getU8G2();

        if (m_ui.getCurrentTime() - timestamp_prev > 500) {
            timestamp_prev = m_ui.getCurrentTime();
            chart_baro.addData(bme.getPressure() / 10.0f);
            chart_temp.addData(bme.getTemperature());
            chart_vol.addData(volumeCtrl.getAirVolume());
        }

        u8g2.setFont(u8g2_font_missingplanet_tr);
        snprintf(print_buffer, sizeof(print_buffer), "%.2fkPa", bme.getPressure() / 10.0f);
        u8g2.drawStr(19 + anim_pressure_x, 15, print_buffer);
        snprintf(print_buffer, sizeof(print_buffer), "%.2fC", bme.getTemperature());
        u8g2.drawStr(19 + anim_temp_x, 39, print_buffer);
        snprintf(print_buffer, sizeof(print_buffer), "%.1fmL", volumeCtrl.getAirVolume());
        u8g2.drawStr(19 + anim_vol_x, 61, print_buffer);

        u8g2.drawXBMP(anim_pressure_x, 2,  13, 12, image_barometer_bits);
        u8g2.drawXBMP(anim_temp_x,     23, 16, 16, image_temperature_bits);
        u8g2.drawXBMP(anim_vol_x,      44, 16, 16, image_volume_bits);

        chart_baro.draw();
        chart_temp.draw();
        chart_vol.draw();
        focusMan.draw();
    }

    bool handleInput(InputEvent event) override {
        IWidget* activeWidget = focusMan.getActiveWidget();
        if (activeWidget) {
            if (activeWidget->handleEvent(event))
                focusMan.clearActiveWidget();
            return true;
        }
        if      (event == InputEvent::BACK)   requestExit();
        else if (event == InputEvent::RIGHT)  focusMan.moveNext();
        else if (event == InputEvent::LEFT)   focusMan.movePrev();
        else if (event == InputEvent::SELECT) focusMan.selectCurrent();
        return true;
    }

    void onExit() override {
        m_ui.setContinousDraw(false);
        m_ui.markFading();
    }
};

AppItem app_environment{
    .title     = "气体状态",
    .bitmap    = image_environment_bits,
    .createApp = [](PixelUI& ui, void*) -> std::shared_ptr<IApplication> {
        return std::make_shared<AppEnvironment>(ui);
    },
};
