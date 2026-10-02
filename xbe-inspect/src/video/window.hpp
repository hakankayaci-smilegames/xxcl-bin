#pragma once

namespace xbe {
namespace video {

bool video_init_window(int width = 640, int height = 480);
void video_poll_events();

} // namespace video
} // namespace xbe
