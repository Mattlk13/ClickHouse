#include "TemporaryReplaceTableName.h"

#include <Common/re2.h>

#include <base/hex.h>
#include <fmt/core.h>


namespace DB
{
    String TemporaryReplaceTableName::toString() const
    {
        return fmt::format("_tmp_replace_{}_{}", getHexUIntLowercase(name_hash), random_suffix);
    }

    std::optional<TemporaryReplaceTableName> TemporaryReplaceTableName::fromString(const String & str)
    {
        static const re2::RE2 pattern("_tmp_replace_(.*)_(.*)");
        TemporaryReplaceTableName result;
        if (RE2::FullMatch(str, pattern, &result.name_hash, &result.random_suffix))
        {
            return result;
        }
        return std::nullopt;
    }
}
