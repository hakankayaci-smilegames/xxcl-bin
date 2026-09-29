#pragma once
#include <string>

namespace xbe {
    bool init_video(const std::string& title);
    void run_video_loop();
    void cleanup_video();
}
