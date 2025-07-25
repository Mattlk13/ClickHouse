#include "config.h"

#if USE_ICU

#include <Functions/FunctionFactory.h>
#include <Functions/FunctionStringToString.h>
#include <Functions/LowerUpperUTF8Impl.h>

namespace DB
{
namespace
{

struct NameUpperUTF8
{
    static constexpr auto name = "upperUTF8";
};

using FunctionUpperUTF8 = FunctionStringToString<LowerUpperUTF8Impl<'a', 'z', true>, NameUpperUTF8>;

}

REGISTER_FUNCTION(UpperUTF8)
{
    FunctionDocumentation::Description description
        = R"(Converts a string to lowercase, assuming that the string contains valid UTF-8 encoded text. If this assumption is violated, no exception is thrown and the result is undefined.)";
    FunctionDocumentation::Syntax syntax = "upperUTF8(input)";
    FunctionDocumentation::Arguments arguments = {{"input", "Input string to convert.", {"String"}}};
    FunctionDocumentation::ReturnedValue returned_value = {"Returns a lowercase string.", {"String"}};
    FunctionDocumentation::Examples examples = { {"first", "SELECT upperUTF8('München') as Upperutf8;", "MÜNCHEN"}, };
    FunctionDocumentation::IntroducedIn introduced_in = {1, 1};
    FunctionDocumentation::Category category = FunctionDocumentation::Category::String;

    factory.registerFunction<FunctionUpperUTF8>({description, syntax, arguments, returned_value, examples, introduced_in, category});
}

}

#endif
