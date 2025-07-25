#include <Compression/ICompressionCodec.h>
#include <Compression/CompressionInfo.h>
#include <Compression/CompressionFactory.h>
#include <base/unaligned.h>
#include <Parsers/IAST.h>
#include <Parsers/ASTLiteral.h>
#include <Parsers/ASTFunction.h>


namespace DB
{

class CompressionCodecDelta : public ICompressionCodec
{
public:
    explicit CompressionCodecDelta(UInt8 delta_bytes_size_);

    uint8_t getMethodByte() const override;

    void updateHash(SipHash & hash) const override;

protected:
    UInt32 doCompressData(const char * source, UInt32 source_size, char * dest) const override;
    void doDecompressData(const char * source, UInt32 source_size, char * dest, UInt32 uncompressed_size) const override;

    UInt32 getMaxCompressedDataSize(UInt32 uncompressed_size) const override { return uncompressed_size + 2; }

    bool isCompression() const override { return false; }
    bool isGenericCompression() const override { return false; }
    bool isDeltaCompression() const override { return true; }

    String getDescription() const override
    {
        return "Preprocessor (should be followed by some compression codec). Stores difference between neighboring values; good for monotonically increasing or decreasing data.";
    }


private:
    const UInt8 delta_bytes_size;
};


namespace ErrorCodes
{
    extern const int CANNOT_COMPRESS;
    extern const int CANNOT_DECOMPRESS;
    extern const int ILLEGAL_SYNTAX_FOR_CODEC_TYPE;
    extern const int ILLEGAL_CODEC_PARAMETER;
    extern const int BAD_ARGUMENTS;
}

CompressionCodecDelta::CompressionCodecDelta(UInt8 delta_bytes_size_)
    : delta_bytes_size(delta_bytes_size_)
{
    setCodecDescription("Delta", {std::make_shared<ASTLiteral>(static_cast<UInt64>(delta_bytes_size))});
}

uint8_t CompressionCodecDelta::getMethodByte() const
{
    return static_cast<uint8_t>(CompressionMethodByte::Delta);
}

void CompressionCodecDelta::updateHash(SipHash & hash) const
{
    getCodecDesc()->updateTreeHash(hash, /*ignore_aliases=*/ true);
}

namespace
{

template <typename T>
void compressDataForType(const char * source, UInt32 source_size, char * dest)
{
    if (source_size % sizeof(T) != 0)
        throw Exception(ErrorCodes::CANNOT_COMPRESS, "Cannot compress with Delta codec, data size {} is not aligned to {}", source_size, sizeof(T));

    T prev_src = 0;
    const char * const source_end = source + source_size;
    while (source < source_end)
    {
        T curr_src = unalignedLoadLittleEndian<T>(source);
        unalignedStoreLittleEndian<T>(dest, curr_src - prev_src);
        prev_src = curr_src;

        source += sizeof(T);
        dest += sizeof(T);
    }
}

template <typename T>
void decompressDataForType(const char * source, UInt32 source_size, char * dest, UInt32 output_size)
{
    const char * const output_end = dest + output_size;

    if (source_size % sizeof(T) != 0)
        throw Exception(ErrorCodes::CANNOT_DECOMPRESS, "Cannot decompress Delta-encoded data, data size {} is not aligned to {}", source_size, sizeof(T));

    T accumulator{};
    const char * const source_end = source + source_size;
    while (source < source_end)
    {
        accumulator += unalignedLoadLittleEndian<T>(source);
        if (dest + sizeof(accumulator) > output_end) [[unlikely]]
            throw Exception(ErrorCodes::CANNOT_DECOMPRESS, "Cannot decompress delta-encoded data");
        unalignedStoreLittleEndian<T>(dest, accumulator);

        source += sizeof(T);
        dest += sizeof(T);
    }
}

}

UInt32 CompressionCodecDelta::doCompressData(const char * source, UInt32 source_size, char * dest) const
{
    UInt8 bytes_to_skip = source_size % delta_bytes_size;
    dest[0] = delta_bytes_size;
    dest[1] = bytes_to_skip; /// unused (backward compatibility)
    memcpy(&dest[2], source, bytes_to_skip);
    size_t start_pos = 2 + bytes_to_skip;
    switch (delta_bytes_size) // NOLINT(bugprone-switch-missing-default-case)
    {
    case 1:
        compressDataForType<UInt8>(&source[bytes_to_skip], source_size - bytes_to_skip, &dest[start_pos]);
        break;
    case 2:
        compressDataForType<UInt16>(&source[bytes_to_skip], source_size - bytes_to_skip, &dest[start_pos]);
        break;
    case 4:
        compressDataForType<UInt32>(&source[bytes_to_skip], source_size - bytes_to_skip, &dest[start_pos]);
        break;
    case 8:
        compressDataForType<UInt64>(&source[bytes_to_skip], source_size - bytes_to_skip, &dest[start_pos]);
        break;
    }
    return 1 + 1 + source_size;
}

void CompressionCodecDelta::doDecompressData(const char * source, UInt32 source_size, char * dest, UInt32 uncompressed_size) const
{
    if (source_size < 2)
        throw Exception(ErrorCodes::CANNOT_DECOMPRESS, "Cannot decompress delta-encoded data. File has wrong header");

    if (uncompressed_size == 0)
        return;

    UInt8 bytes_size = source[0];

    if (!(bytes_size == 1 || bytes_size == 2 || bytes_size == 4 || bytes_size == 8))
        throw Exception(ErrorCodes::CANNOT_DECOMPRESS, "Cannot decompress delta-encoded data. File has wrong header");

    UInt8 bytes_to_skip = uncompressed_size % bytes_size;
    UInt32 output_size = uncompressed_size - bytes_to_skip;

    if (static_cast<UInt32>(2 + bytes_to_skip) > source_size)
        throw Exception(ErrorCodes::CANNOT_DECOMPRESS, "Cannot decompress delta-encoded data. File has wrong header");

    memcpy(dest, &source[2], bytes_to_skip);
    UInt32 source_size_no_header = source_size - bytes_to_skip - 2;
    switch (bytes_size) // NOLINT(bugprone-switch-missing-default-case)
    {
    case 1:
        decompressDataForType<UInt8>(&source[2 + bytes_to_skip], source_size_no_header, &dest[bytes_to_skip], output_size);
        break;
    case 2:
        decompressDataForType<UInt16>(&source[2 + bytes_to_skip], source_size_no_header, &dest[bytes_to_skip], output_size);
        break;
    case 4:
        decompressDataForType<UInt32>(&source[2 + bytes_to_skip], source_size_no_header, &dest[bytes_to_skip], output_size);
        break;
    case 8:
        decompressDataForType<UInt64>(&source[2 + bytes_to_skip], source_size_no_header, &dest[bytes_to_skip], output_size);
        break;
    }
}

namespace
{

UInt8 getDeltaBytesSize(const IDataType * column_type)
{
    if (!column_type->isValueUnambiguouslyRepresentedInFixedSizeContiguousMemoryRegion())
        throw Exception(ErrorCodes::BAD_ARGUMENTS, "Codec Delta is not applicable for {} because the data type is not of fixed size",
            column_type->getName());

    size_t max_size = column_type->getSizeOfValueInMemory();
    if (max_size == 1 || max_size == 2 || max_size == 4 || max_size == 8)
        return static_cast<UInt8>(max_size);
    throw Exception(
        ErrorCodes::BAD_ARGUMENTS,
        "Codec Delta is only applicable for data types of size 1, 2, 4, 8 bytes. Given type {}",
        column_type->getName());
}

}

void registerCodecDelta(CompressionCodecFactory & factory)
{
    UInt8 method_code = static_cast<UInt8>(CompressionMethodByte::Delta);
    auto codec_builder = [&](const ASTPtr & arguments, const IDataType * column_type) -> CompressionCodecPtr
    {
        /// Default bytes size is 1.
        UInt8 delta_bytes_size = 1;

        if (arguments && !arguments->children.empty())
        {
            if (arguments->children.size() > 1)
                throw Exception(ErrorCodes::ILLEGAL_SYNTAX_FOR_CODEC_TYPE, "Delta codec must have 1 parameter, given {}", arguments->children.size());

            const auto children = arguments->children;
            const auto * literal = children[0]->as<ASTLiteral>();
            if (!literal || literal->value.getType() != Field::Types::Which::UInt64)
                throw Exception(ErrorCodes::ILLEGAL_CODEC_PARAMETER, "Delta codec argument must be unsigned integer");

            size_t user_bytes_size = literal->value.safeGet<UInt64>();
            if (user_bytes_size != 1 && user_bytes_size != 2 && user_bytes_size != 4 && user_bytes_size != 8)
                throw Exception(ErrorCodes::ILLEGAL_CODEC_PARAMETER, "Delta value for delta codec can be 1, 2, 4 or 8, given {}", user_bytes_size);
            delta_bytes_size = static_cast<UInt8>(user_bytes_size);
        }
        else if (column_type)
        {
            delta_bytes_size = getDeltaBytesSize(column_type);
        }

        return std::make_shared<CompressionCodecDelta>(delta_bytes_size);
    };
    factory.registerCompressionCodecWithType("Delta", method_code, codec_builder);
}

CompressionCodecPtr getCompressionCodecDelta(UInt8 delta_bytes_size)
{
    return std::make_shared<CompressionCodecDelta>(delta_bytes_size);
}

}
