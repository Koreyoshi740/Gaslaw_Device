#pragma once

#include "PopupBase.h"
#include "StatusEntry.h"

/**
 * @class PopupStatus
 * @brief A scrollable, real-time status display popup.
 *
 * Displays a list of label/value pairs where values are generated live
 * via formatter callbacks. LEFT/RIGHT scroll the list; SELECT closes.
 */
class PopupStatus : public PopupBase {
public:
    PopupStatus(PixelUI& ui, uint16_t width, uint16_t height,
                const StatusEntry* entries, uint8_t count,
                const char* title = "",
                uint16_t duration = 30000, uint8_t priority = 0);
    ~PopupStatus() = default;

    void drawContent(int16_t cx, int16_t cy, int16_t cw, int16_t ch) override;
    bool handleInput(InputEvent event) override;

private:
    const StatusEntry* _entries;
    uint8_t _count;
    uint8_t _scroll = 0;
    int32_t _scrollPx = 0;
    int8_t  _visLines = 0;
    const char* _title;
};
