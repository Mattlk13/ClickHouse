#pragma once

#include <Columns/IColumn.h>
#include <Interpreters/AggregationCommon.h>

namespace DB
{

namespace ColumnsHashing
{

namespace columns_hashing_impl
{

template <typename Value, bool consecutive_keys_optimization_>
struct LastElementCache
{
    static constexpr bool consecutive_keys_optimization = consecutive_keys_optimization_;
    Value value;
    bool empty = true;
    bool found = false;

    void setValue(const Value & new_value)
    {
        value = new_value;
        empty = false;
        found = true;
    }

    template <typename Key>
    void setNotFound(const Key & key)
    {
        value.first = key;
        empty = false;
        found = false;
    }

    void setNotFound(const Value & value_)
    {
        value = value_;
        empty = false;
        found = false;
    }
};

template <typename Data>
struct LastElementCache<Data, false>
{
    static constexpr bool consecutive_keys_optimization = false;
};

template <typename Value, typename Cache>
class EmplaceResultImpl
{
    bool inserted;
public:
    template <typename iterator>
    EmplaceResultImpl(iterator & it, Cache & cache, bool inserted) : inserted(inserted)
    {
        if constexpr (Cache::consecutive_keys_optimization)
            cache.setValue(*it);
    }
    bool isInserted() const { return inserted; }
};

template <typename First, typename Second, typename Cache>
class EmplaceResultImpl<PairNoInit<First, Second>, Cache>
{
    using Value = PairNoInit<First, Second>;
    Value & value;
    Cache & cache;
    bool inserted;

public:
    explicit EmplaceResultImpl(typename std::enable_if<Cache::consecutive_keys_optimization, Cache>::type & cache)
        : value(cache.value), cache(cache), inserted(false) {}

    EmplaceResultImpl(Value & value, Cache & cache, bool inserted) : value(value), cache(cache), inserted(inserted)
    {
        if constexpr (Cache::consecutive_keys_optimization)
            cache.setValue(value);
    }

    bool isInserted() const { return inserted; }
    const auto & getMapped() const { return value.second; }

    void setMapped(const Second & mapped)
    {
        value.second = mapped;

        if constexpr (Cache::consecutive_keys_optimization)
            cache.setValue(value);
    }
};

template <typename Value, typename Cache>
class FindResultImpl
{
    bool found;

public:
    template <typename Key, typename iterator>
    FindResultImpl(Key & key, iterator & it, Cache & cache, bool found) : found(found)
    {
        if constexpr (Cache::consecutive_keys_optimization)
        {
            if (found)
                cache.setValue(*it);
            else
                cache.setNotFound(key);
        }
    }

    bool isFound() const { return found; }
};

template <typename First, typename Second, typename Cache>
class FindResultImpl<PairNoInit<First, Second>, Cache>
{
    using Value = PairNoInit<First, Second>;
    Value & value;
    Cache & cache;
    bool found;

public:
    explicit FindResultImpl(typename std::enable_if<Cache::consecutive_keys_optimization, Cache>::type & cache)
        : value(cache.value), cache(cache), found(cache.found) {}

    FindResultImpl(First & key, Value * value_ptr, Cache & cache, bool found) : cache(cache), found(found)
    {
        if (found)
            value = *value_ptr;
        else
            value.first = key;

        if constexpr (Cache::consecutive_keys_optimization)
        {
            if (found)
                cache.setValue(value);
            else
                cache.setNotFound(key);
        }
    }

    bool isFound() const { return found; }
    const auto & getMapped() const { return value.second; }
};

template <typename Value, bool consecutive_keys_optimization>
struct HashMethodBase
{
    using Cache = LastElementCache<Value, consecutive_keys_optimization>;
    using EmplaceResult = EmplaceResultImpl<Value, Cache>;
    using FindResult = FindResultImpl<Value, Cache>;

    Cache cache;

protected:
    template <typename Data, typename Key>
    EmplaceResult emplaceKeyImpl(Key key, Data & data)
    {
        if constexpr (Cache::consecutive_keys_optimization)
        {
            if (!cache.empty && cache.found && cache.getKey() == key)
                return EmplaceResult(cache);
        }

        bool inserted = false;
        typename Data::iterator it;
        data.emplace(key, it, inserted);

        return EmplaceRes(*it, cache, inserted);
    }

    template <typename Data, typename Key>
    FindResult findKeyImpl(Key key, Data & data)
    {
        if constexpr (Cache::consecutive_keys_optimization)
        {
            if (!cache.empty && cache.getKey() == key)
                return FindRes(cache);
        }

        auto it = data.find(key);

        if (it == data.end())
            return FindRes(key, nullptr, cache, false);
        else
            return FindRes(key, &(*it), cache, true);
    }
};


/// This class is designed to provide the functionality that is required for
/// supporting nullable keys in HashMethodKeysFixed. If there are
/// no nullable keys, this class is merely implemented as an empty shell.
template <typename Key, bool has_nullable_keys>
class BaseStateKeysFixed;

/// Case where nullable keys are supported.
template <typename Key>
class BaseStateKeysFixed<Key, true>
{
protected:
    void init(const ColumnRawPtrs & key_columns)
    {
        null_maps.reserve(key_columns.size());
        actual_columns.reserve(key_columns.size());

        for (const auto & col : key_columns)
        {
            if (col->isColumnNullable())
            {
                const auto & nullable_col = static_cast<const ColumnNullable &>(*col);
                actual_columns.push_back(&nullable_col.getNestedColumn());
                null_maps.push_back(&nullable_col.getNullMapColumn());
            }
            else
            {
                actual_columns.push_back(col);
                null_maps.push_back(nullptr);
            }
        }
    }

    /// Return the columns which actually contain the values of the keys.
    /// For a given key column, if it is nullable, we return its nested
    /// column. Otherwise we return the key column itself.
    inline const ColumnRawPtrs & getActualColumns() const
    {
        return actual_columns;
    }

    /// Create a bitmap that indicates whether, for a particular row,
    /// a key column bears a null value or not.
    KeysNullMap<Key> createBitmap(size_t row) const
    {
        KeysNullMap<Key> bitmap{};

        for (size_t k = 0; k < null_maps.size(); ++k)
        {
            if (null_maps[k] != nullptr)
            {
                const auto & null_map = static_cast<const ColumnUInt8 &>(*null_maps[k]).getData();
                if (null_map[row] == 1)
                {
                    size_t bucket = k / 8;
                    size_t offset = k % 8;
                    bitmap[bucket] |= UInt8(1) << offset;
                }
            }
        }

        return bitmap;
    }

private:
    ColumnRawPtrs actual_columns;
    ColumnRawPtrs null_maps;
};

/// Case where nullable keys are not supported.
template <typename Key>
class BaseStateKeysFixed<Key, false>
{
protected:
    void init(const ColumnRawPtrs & columns) { actual_columns = columns; }

    const ColumnRawPtrs & getActualColumns() const { return actual_columns; }

    KeysNullMap<Key> createBitmap(size_t) const
    {
        throw Exception{"Internal error: calling createBitmap() for non-nullable keys"
                        " is forbidden", ErrorCodes::LOGICAL_ERROR};
    }

private:
    ColumnRawPtrs actual_columns;
};

}

}

}
