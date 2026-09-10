#pragma once
namespace konbini::app {
// Raw contact state in framebuffer pixels after the adapter's coordinate conversion.
struct PointerSample {
    double xPixels=0, yPixels=0, pressXPixels=0, pressYPixels=0;
    double deltaXPixels=0, deltaYPixels=0, pinchRatio=1;
    bool down=false, pressed=false, released=false, cancelled=false;
    bool isTouch=false, multipleContacts=false;
};
}
