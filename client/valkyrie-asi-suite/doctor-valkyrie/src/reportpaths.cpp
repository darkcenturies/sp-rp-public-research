#include "reportpaths.h"

#include <cctype>
#include <cstring>

namespace reportpaths {
namespace {

bool EqualInsensitive(char a, char b) {
    if (a == '/') a = '\\';
    if (b == '/') b = '\\';
    return std::tolower(static_cast<unsigned char>(a)) ==
           std::tolower(static_cast<unsigned char>(b));
}

bool StartsWithPath(const char* value, const char* prefix) {
    if (!value || !prefix || !*prefix) return false;
    size_t i = 0;
    for (; prefix[i]; ++i) {
        if (!value[i] || !EqualInsensitive(value[i], prefix[i])) return false;
    }
    return value[i] == '\0' || value[i] == '\\' || value[i] == '/';
}

}  // namespace

std::string RedactUserProfile(const char* text, const char* userProfile) {
    std::string out = text ? text : "";
    if (!userProfile || !*userProfile) return out;
    const size_t profileLength = strlen(userProfile);
    for (size_t at = 0; at + profileLength <= out.size();) {
        bool match = true;
        for (size_t i = 0; i < profileLength; ++i) {
            if (!EqualInsensitive(out[at + i], userProfile[i])) {
                match = false;
                break;
            }
        }
        if (match &&
            (at + profileLength == out.size() ||
             out[at + profileLength] == '\\' ||
             out[at + profileLength] == '/')) {
            out.replace(at, profileLength, "<USERPROFILE>");
            at += strlen("<USERPROFILE>");
        } else {
            ++at;
        }
    }
    return out;
}

bool IsGameLocal(const char* path, const char* gameRoot) {
    if (!path || !*path) return true;
    const bool driveAbsolute = std::isalpha(static_cast<unsigned char>(path[0])) &&
                               path[1] == ':';
    const bool unc = (path[0] == '\\' && path[1] == '\\') ||
                     (path[0] == '/' && path[1] == '/');
    if (!driveAbsolute && !unc) return true;
    return StartsWithPath(path, gameRoot);
}

}  // namespace reportpaths
