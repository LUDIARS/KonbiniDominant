#pragma once
#include "konbini/app/frame_input.h"
#include "konbini/app/touch_contacts.h"
namespace konbini::web {
class BrowserInput {
public:
    void pointer(int id, int phase, double x, double y, bool touch);
    void wheel(double steps);
    void cancel() noexcept;
    [[nodiscard]] app::FrameInput consume(double dt);
private:
    app::TouchContacts contacts_;
    double wheel_ = 0;
    bool touch_ = false;
};
}
