#ifndef __clang_analyzer__ // It's too hard to analyze.

#include <Functions/GatherUtils/GatherUtils.h>
#include <Functions/GatherUtils/Selectors.h>
#include <Functions/GatherUtils/Algorithms.h>

namespace DB::GatherUtils
{

namespace
{

struct SliceFromRightConstantOffsetBoundedSelectArraySource
    : public ArraySourceSelector<SliceFromRightConstantOffsetBoundedSelectArraySource>
{
    template <typename Source>
    static void selectSource(bool is_const, bool is_nullable, Source && source, size_t & offset, ssize_t & length, ColumnArray::MutablePtr & result)
    {
        using SourceType = typename std::decay<Source>::type;
        using Sink = typename SourceType::SinkType;

        if (is_nullable)
        {
            using NullableSource = NullableArraySource<SourceType>;
            using NullableSink = typename NullableSource::SinkType;

            auto & nullable_source = static_cast<NullableSource &>(source);

            result = ColumnArray::create(nullable_source.createValuesColumn());
            NullableSink sink(result->getData(), result->getOffsets(), source.getColumnSize());

            if (is_const)
                sliceFromRightConstantOffsetBounded(static_cast<ConstSource<NullableSource> &>(source), sink, offset, length);
            else
                sliceFromRightConstantOffsetBounded(static_cast<NullableSource &>(source), sink, offset, length);
        }
        else
        {
            result = ColumnArray::create(source.createValuesColumn());
            Sink sink(result->getData(), result->getOffsets(), source.getColumnSize());

            if (is_const)
                sliceFromRightConstantOffsetBounded(static_cast<ConstSource<SourceType> &>(source), sink, offset, length);
            else
                sliceFromRightConstantOffsetBounded(source, sink, offset, length);
        }
    }
};

}

ColumnArray::MutablePtr sliceFromRightConstantOffsetBounded(IArraySource & src, size_t offset, ssize_t length)
{
    ColumnArray::MutablePtr res;
    SliceFromRightConstantOffsetBoundedSelectArraySource::select(src, offset, length, res);
    return res;
}
}

#endif
