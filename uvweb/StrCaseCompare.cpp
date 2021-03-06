
#include "StrCaseCompare.h"

#include <algorithm>
#include <locale>

namespace uvweb
{
    bool CaseInsensitiveLess::NocaseCompare::operator()(const unsigned char& c1,
                                                        const unsigned char& c2) const
    {
#ifdef _WIN32
        return std::tolower(c1, std::locale()) < std::tolower(c2, std::locale());
#else
        return std::tolower(c1) < std::tolower(c2);
#endif
    }

    bool CaseInsensitiveLess::cmp(const std::string& s1, const std::string& s2)
    {
        return std::lexicographical_compare(s1.begin(),
                                            s1.end(), // source range
                                            s2.begin(),
                                            s2.end(),         // dest range
                                            NocaseCompare()); // comparison
    }

    bool CaseInsensitiveLess::operator()(const std::string& s1, const std::string& s2) const
    {
        return CaseInsensitiveLess::cmp(s1, s2);
    }
} // namespace uvweb
