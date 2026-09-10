#include "cleowatch.h"

#include <windows.h>

#include <algorithm>
#include <cctype>

namespace cleowatch {
namespace {

bool Suspended(const std::string& line) {
    std::string lower = line;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(tolower(c));
    });
    return lower.find("script suspended") != std::string::npos;
}

}  // namespace

void Watcher::Start(const char* path) {
    path_ = path ? path : "";
    pending_.clear();
    context_.clear();
    offset_ = 0;
    started_ = true;

    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (GetFileAttributesExA(path_.c_str(), GetFileExInfoStandard, &data)) {
        offset_ = (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) |
                  data.nFileSizeLow;
    }
}

bool Watcher::Poll(std::string* event) {
    if (!started_ || path_.empty() || !event) return false;
    HANDLE file = CreateFileA(path_.c_str(), GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size)) {
        CloseHandle(file);
        return false;
    }
    const std::uint64_t length = static_cast<std::uint64_t>(size.QuadPart);
    if (length < offset_) {
        // CLEO recreated/truncated the log after Doctor started.
        offset_ = 0;
        pending_.clear();
        context_.clear();
    }
    if (length != offset_) {
        LARGE_INTEGER position{};
        position.QuadPart = static_cast<LONGLONG>(offset_);
        SetFilePointerEx(file, position, nullptr, FILE_BEGIN);
        char chunk[4096];
        while (offset_ < length) {
            const DWORD wanted = static_cast<DWORD>((std::min<std::uint64_t>)(
                sizeof(chunk), length - offset_));
            DWORD read = 0;
            if (!ReadFile(file, chunk, wanted, &read, nullptr) || read == 0) break;
            pending_.append(chunk, read);
            offset_ += read;
        }
    }
    CloseHandle(file);

    size_t newline = 0;
    while ((newline = pending_.find('\n')) != std::string::npos) {
        std::string line = pending_.substr(0, newline);
        pending_.erase(0, newline + 1);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (Suspended(line)) {
            *event = context_;
            if (!event->empty()) *event += "\n";
            *event += line;
            context_.clear();
            return true;
        }
        if (!line.empty()) {
            context_ += line + "\n";
            // Enough context for wrapped CLEO errors without retaining the log.
            while (std::count(context_.begin(), context_.end(), '\n') > 4) {
                context_.erase(0, context_.find('\n') + 1);
            }
        }
    }
    return false;
}

}  // namespace cleowatch
