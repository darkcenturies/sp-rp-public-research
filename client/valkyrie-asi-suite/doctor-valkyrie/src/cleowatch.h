#pragma once

#include <cstdint>
#include <string>

namespace cleowatch {

// Tails CLEO's own log without modifying it. Existing content is ignored when
// monitoring starts; only errors written during this game launch are emitted.
class Watcher {
public:
    void Start(const char* path);
    bool Poll(std::string* event);

private:
    std::string path_;
    std::string pending_;
    std::string context_;
    std::uint64_t offset_ = 0;
    bool started_ = false;
};

}  // namespace cleowatch
