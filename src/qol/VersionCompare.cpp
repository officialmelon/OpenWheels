#include "qol/VersionCompare.h"

#include <algorithm>

namespace qol {

std::vector<int> versionParts(const std::string& version)
{
    std::vector<int> parts;
    size_t i = 0;
    if (i < version.size() && (version[i] == 'v' || version[i] == 'V')) ++i;
    int value = 0;
    bool any = false;
    for (; i < version.size(); ++i)
    {
        const char c = version[i];
        if (c >= '0' && c <= '9')
        {
            value = value * 10 + (c - '0');
            any = true;
        }
        else if (c == '.')
        {
            parts.push_back(value);
            value = 0;
            any = false;
        }
        else
        {
            break;  // "-beta.3": only the numeric core counts
        }
    }
    if (any) parts.push_back(value);
    return parts;
}

bool isNewerVersion(const std::string& latest, const std::string& current)
{
    const std::vector<int> a = versionParts(latest), b = versionParts(current);
    if (a.empty() || b.empty()) return false;
    for (size_t i = 0; i < std::max(a.size(), b.size()); ++i)
    {
        const int x = i < a.size() ? a[i] : 0, y = i < b.size() ? b[i] : 0;
        if (x != y) return x > y;
    }
    return false;
}

}  // namespace qol
