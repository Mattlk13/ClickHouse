#include <Parsers/Access/ASTCreateRowPolicyQuery.h>
#include <Parsers/Access/ASTRolesOrUsersSet.h>
#include <Parsers/Access/ASTRowPolicyName.h>
#include <Common/quoteString.h>
#include <IO/Operators.h>
#include <boost/container/flat_set.hpp>


namespace DB
{
namespace
{
    void formatRenameTo(const String & new_short_name, WriteBuffer & ostr, const IAST::FormatSettings &)
    {
        ostr << " RENAME TO " << backQuote(new_short_name);
    }


    void formatAsRestrictiveOrPermissive(bool is_restrictive, WriteBuffer & ostr, const IAST::FormatSettings &)
    {
        ostr << " AS " << (is_restrictive ? "restrictive" : "permissive");
    }


    void formatFilterExpression(const ASTPtr & expr, WriteBuffer & ostr, const IAST::FormatSettings & settings)
    {
        ostr << " ";
        if (expr)
            expr->format(ostr, settings);
        else
            ostr << "NONE";
    }


    void formatForClause(const boost::container::flat_set<std::string_view> & commands, const String & filter, const String & check, bool alter, WriteBuffer & ostr, const IAST::FormatSettings &)
    {
        ostr << " FOR ";
        bool need_comma = false;
        for (const auto & command : commands)
        {
            if (std::exchange(need_comma, true))
                ostr << ", ";
            ostr << command;
        }

        if (!filter.empty())
            ostr << " USING" << filter;

        if (!check.empty() && (alter || (check != filter)))
            ostr << " WITH CHECK" << check;
    }


    void formatForClauses(const std::vector<std::pair<RowPolicyFilterType, ASTPtr>> & filters, bool alter, WriteBuffer & ostr, const IAST::FormatSettings & settings)
    {
        std::vector<std::pair<RowPolicyFilterType, String>> filters_as_strings;
        WriteBufferFromOwnString temp_buf;
        for (const auto & [filter_type, filter] : filters)
        {
            formatFilterExpression(filter, temp_buf, settings);
            filters_as_strings.emplace_back(filter_type, temp_buf.str());
            temp_buf.restart();
        }

        boost::container::flat_set<std::string_view> commands;
        String filter;
        String check;

        do
        {
            commands.clear();
            filter.clear();
            check.clear();

            /// Collect commands using the same filter and check conditions.
            for (auto & [filter_type, str] : filters_as_strings)
            {
                if (str.empty())
                    continue;
                const auto & type_info = RowPolicyFilterTypeInfo::get(filter_type);
                if (type_info.is_check)
                {
                    if (check.empty())
                        check = str;
                    else if (check != str)
                        continue;
                }
                else
                {
                    if (filter.empty())
                        filter = str;
                    else if (filter != str)
                        continue;
                }
                commands.emplace(type_info.command);
                str.clear(); /// Skip this condition on the next iteration.
            }

            if (!filter.empty() || !check.empty())
                formatForClause(commands, filter, check, alter, ostr, settings);
        }
        while (!filter.empty() || !check.empty());
    }


    void formatToRoles(const ASTRolesOrUsersSet & roles, WriteBuffer & ostr, const IAST::FormatSettings & settings)
    {
        ostr << " TO ";
        roles.format(ostr, settings);
    }
}


String ASTCreateRowPolicyQuery::getID(char) const
{
    return "CREATE ROW POLICY or ALTER ROW POLICY query";
}


ASTPtr ASTCreateRowPolicyQuery::clone() const
{
    auto res = std::make_shared<ASTCreateRowPolicyQuery>(*this);

    if (names)
        res->names = std::static_pointer_cast<ASTRowPolicyNames>(names->clone());

    if (roles)
        res->roles = std::static_pointer_cast<ASTRolesOrUsersSet>(roles->clone());

    /// `res->filters` is already initialized by the copy constructor of ASTCreateRowPolicyQuery (see the first line of this function).
    /// But the copy constructor just copied the pointers inside `filters` instead of cloning.
    /// We need to make a deep copy and not a shallow copy, so we have to manually clone each pointer in `res->filters`.
    chassert(res->filters.size() == filters.size());
    for (auto & [_, res_filter] : res->filters)
    {
        if (res_filter)
            res_filter = res_filter->clone();
    }

    return res;
}


void ASTCreateRowPolicyQuery::formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState &, FormatStateStacked) const
{
    if (attach)
    {
        ostr << "ATTACH ROW POLICY";
    }
    else
    {
        ostr << (alter ? "ALTER ROW POLICY" : "CREATE ROW POLICY")
                     ;
    }

    if (if_exists)
        ostr << " IF EXISTS";
    else if (if_not_exists)
        ostr << " IF NOT EXISTS";
    else if (or_replace)
        ostr << " OR REPLACE";

    ostr << " ";
    names->format(ostr, settings);

    if (!storage_name.empty())
        ostr
                    << " IN "
                    << backQuoteIfNeed(storage_name);

    formatOnCluster(ostr, settings);
    assert(names->cluster.empty());

    if (!new_short_name.empty())
        formatRenameTo(new_short_name, ostr, settings);

    if (is_restrictive)
        formatAsRestrictiveOrPermissive(*is_restrictive, ostr, settings);

    formatForClauses(filters, alter, ostr, settings);

    if (roles && (!roles->empty() || alter))
        formatToRoles(*roles, ostr, settings);
}


void ASTCreateRowPolicyQuery::replaceCurrentUserTag(const String & current_user_name) const
{
    if (roles)
        roles->replaceCurrentUserTag(current_user_name);
}

void ASTCreateRowPolicyQuery::replaceEmptyDatabase(const String & current_database) const
{
    if (names)
        names->replaceEmptyDatabase(current_database);
}
}
