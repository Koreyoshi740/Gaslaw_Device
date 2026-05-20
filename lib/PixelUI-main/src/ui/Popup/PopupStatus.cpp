#include "ui/Popup/PopupStatus.h"
#include "PixelUI.h"
#include <cstring>
#include <cstdio>
#include <algorithm>

PopupStatus::PopupStatus(PixelUI& ui, uint16_t width, uint16_t height,
                         const StatusEntry* entries, uint8_t count,
                         const char* title, uint16_t duration, uint8_t priority)
    : PopupBase(ui, width, height, priority, duration),
      _entries(entries), _count(count), _title(title)
{
}

void PopupStatus::drawContent(int16_t cx, int16_t cy, int16_t cw, int16_t ch) {
    U8G2& u8g2 = m_ui.getU8G2();

    const int16_t LINE_H  = 13;
    const int16_t MARGIN  = 2;
    int16_t top    = cy - ch / 2 + MARGIN;
    int16_t left   = cx - cw / 2 + MARGIN;
    int16_t right  = cx + cw / 2 - MARGIN;
    int16_t bottom = cy + ch / 2 - MARGIN;

    u8g2.setFont(u8g2_font_wqy12_t_gb2312);

    if (_title && _title[0]) {
        int16_t tw = u8g2.getUTF8Width(_title);
        u8g2.drawUTF8(cx - tw / 2, top + 11, _title);
        top += LINE_H;  // no extra gap — maximise content rows
    }

    int16_t available = bottom - top;
    _visLines = available / LINE_H;

    // clip to content area — partially-visible rows get pixel-clipped at boundary
    u8g2.setClipWindow(left, top, right - 1, bottom - 1);

    const int16_t VAL_RIGHT = right - 3;  // 3px padding from right edge
    char vbuf[24];
    for (uint8_t i = 0; i < _count; i++) {
        int16_t itemY = (int16_t)(i * LINE_H) - (int16_t)_scrollPx;
        if (itemY >= available) break;
        if (itemY + LINE_H <= 0) continue;
        int16_t y = top + itemY + 11;
        if (_entries[i].label)
            u8g2.drawUTF8(left, y, _entries[i].label);
        if (_entries[i].formatter) {
            vbuf[0] = '\0';
            _entries[i].formatter(vbuf, sizeof(vbuf));
            int16_t vw = u8g2.getUTF8Width(vbuf);
            u8g2.drawUTF8(VAL_RIGHT - vw, y, vbuf);
        }
    }

    u8g2.setMaxClipWindow();

    // scroll indicators drawn after restoring clip
    if (_scroll > 0)
        u8g2.drawStr(right - 4, cy - ch / 2 + 7, "^");
    if (_visLines > 0 && _scroll + (uint8_t)_visLines < _count)
        u8g2.drawStr(right - 4, cy + ch / 2 - 2, "v");

    m_ui.markDirty();
}

bool PopupStatus::handleInput(InputEvent event) {
    if (_state == PopupState::CLOSING) return true;

    switch (event) {
        case InputEvent::LEFT:
            if (_scroll > 0) {
                _scroll--;
                _startTime = m_ui.getCurrentTime();
                m_ui.animate(_scrollPx, (int32_t)_scroll * 13, 150, EasingType::EASE_OUT_CUBIC);
            }
            return true;

        case InputEvent::RIGHT: {
            uint8_t maxScroll = (_visLines > 0 && _count > (uint8_t)_visLines)
                                ? _count - (uint8_t)_visLines : 0;
            if (_scroll < maxScroll) {
                _scroll++;
                _startTime = m_ui.getCurrentTime();
                m_ui.animate(_scrollPx, (int32_t)_scroll * 13, 150, EasingType::EASE_OUT_CUBIC);
            }
            return true;
        }

        case InputEvent::SELECT:
        case InputEvent::BACK:
            startClosingAnimation();
            return true;

        default:
            return true;  // swallow all other keys without closing
    }
}
