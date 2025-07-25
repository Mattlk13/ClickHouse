#include <Processors/Merges/Algorithms/Graphite.h>
#include <Processors/Merges/Algorithms/GraphiteRollupSortedAlgorithm.h>
#include <AggregateFunctions/IAggregateFunction.h>
#include <Common/DateLUTImpl.h>
#include <Common/DateLUT.h>
#include <Core/Block.h>


namespace DB
{

namespace ErrorCodes
{
    extern const int BAD_ARGUMENTS;
}


static GraphiteRollupSortedAlgorithm::ColumnsDefinition defineColumns(
    const Block & header, const Graphite::Params & params)
{
    GraphiteRollupSortedAlgorithm::ColumnsDefinition def;

    def.path_column_num = header.getPositionByName(params.path_column_name);
    def.time_column_num = header.getPositionByName(params.time_column_name);
    def.value_column_num = header.getPositionByName(params.value_column_name);
    def.version_column_num = header.getPositionByName(params.version_column_name);

    def.time_column_type = header.getByPosition(def.time_column_num).type;

    size_t num_columns = header.columns();
    for (size_t i = 0; i < num_columns; ++i)
        if (i != def.time_column_num && i != def.value_column_num && i != def.version_column_num)
            def.unmodified_column_numbers.push_back(i);

    if (!WhichDataType(header.getByPosition(def.value_column_num).type).isFloat64())
        throw Exception(ErrorCodes::BAD_ARGUMENTS, "Only `Float64` data type is allowed for the value column of GraphiteMergeTree");

    return def;
}

GraphiteRollupSortedAlgorithm::GraphiteRollupSortedAlgorithm(
    SharedHeader header_,
    size_t num_inputs,
    SortDescription description_,
    size_t max_block_size_rows_,
    size_t max_block_size_bytes_,
    Graphite::Params params_,
    time_t time_of_merge_)
    : IMergingAlgorithmWithSharedChunks(header_, num_inputs, std::move(description_), nullptr, max_row_refs, std::make_unique<GraphiteRollupMergedData>(false, max_block_size_rows_, max_block_size_bytes_))
    , graphite_rollup_merged_data(assert_cast<GraphiteRollupMergedData &>(*merged_data))
    , params(std::move(params_))
    , time_of_merge(time_of_merge_)
{
    size_t max_size_of_aggregate_state = 0;
    size_t max_alignment_of_aggregate_state = 1;

    for (const auto & pattern : params.patterns)
    {
        if (pattern.function)
        {
            max_size_of_aggregate_state = std::max(max_size_of_aggregate_state, pattern.function->sizeOfData());
            max_alignment_of_aggregate_state = std::max(max_alignment_of_aggregate_state, pattern.function->alignOfData());
        }
    }

    graphite_rollup_merged_data.allocMemForAggregates(max_size_of_aggregate_state, max_alignment_of_aggregate_state);
    columns_definition = defineColumns(*header_, params);
}

UInt32 GraphiteRollupSortedAlgorithm::selectPrecision(const Graphite::Retentions & retentions, time_t time) const
{
    static_assert(is_signed_v<time_t>, "time_t must be signed type");

    for (const auto & retention : retentions)
    {
        if (time_of_merge - time >= static_cast<time_t>(retention.age))
            return retention.precision;
    }

    /// No rounding.
    return 1;
}

/** Round the unix timestamp to seconds precision.
  * In this case, the date should not change. The date is calculated using the local time zone.
  *
  * If the rounding value is less than an hour,
  *  then, assuming that time zones that differ from UTC by a multiple of 15-minute intervals
  *  (that is true for all modern timezones but not true for historical timezones).
  * And if the rounding value is greater,
  *  then we will round down the number of seconds from the beginning of the day in the local time zone.
  *
  * Rounding to more than a day is not supported.
  */
static time_t roundTimeToPrecision(const DateLUTImpl & date_lut, time_t time, UInt32 precision)
{
    if (precision <= 900)
    {
        return time / precision * precision;
    }

    time_t date = date_lut.toDate(time);
    time_t remainder = time - date;
    return date + remainder / precision * precision;
}

IMergingAlgorithm::Status GraphiteRollupSortedAlgorithm::merge()
{
    /// Timestamp column can be DateTime or UInt32. If it is DateTime, we can use its timezone for calculations.
    const TimezoneMixin * timezone = dynamic_cast<const TimezoneMixin *>(columns_definition.time_column_type.get());

    const DateLUTImpl & date_lut = timezone ? timezone->getTimeZone() : DateLUT::instance();

    /// Take rows in needed order and put them into `graphite_rollup_merged_data` until we get `max_block_size` rows.
    ///
    /// Variables starting with current_* refer to the rows previously popped from the queue that will
    /// contribute towards current output row.
    /// Variables starting with next_* refer to the row at the top of the queue.

    while (queue.isValid())
    {
        SortCursor current = queue.current();

        if (current->isLast() && skipLastRowFor(current->order))
        {
            /// Get the next block from the corresponding source, if there is one.
            queue.removeTop();
            return Status(current.impl->order);
        }

        std::string_view next_path = current->all_columns[columns_definition.path_column_num]->getDataAt(current->getRow()).toView();
        bool new_path = is_first || next_path != current_group_path;

        is_first = false;

        time_t next_row_time = current->all_columns[columns_definition.time_column_num]->getUInt(current->getRow());
        /// Is new key before rounding.
        bool is_new_key = new_path || next_row_time != current_time;

        if (is_new_key)
        {
            /// Accumulate the row that has maximum version in the previous group of rows with the same key:
            if (graphite_rollup_merged_data.wasGroupStarted())
                accumulateRow(current_subgroup_newest_row);

            Graphite::RollupRule next_rule = graphite_rollup_merged_data.currentRule();
            if (new_path)
                next_rule = selectPatternForPath(this->params, next_path);

            const Graphite::RetentionPattern * retention_pattern = std::get<0>(next_rule);
            time_t next_time_rounded;
            if (retention_pattern)
            {
                UInt32 precision = selectPrecision(retention_pattern->retentions, next_row_time);
                next_time_rounded = roundTimeToPrecision(date_lut, next_row_time, precision);
            }
            else
            {
                /// If no pattern has matched - take the value as-is.
                next_time_rounded = next_row_time;
            }

            /// Key will be new after rounding. It means new result row.
            bool will_be_new_key = new_path || next_time_rounded != current_time_rounded;

            if (will_be_new_key)
            {
                if (graphite_rollup_merged_data.wasGroupStarted())
                {
                    finishCurrentGroup();

                    /// We have enough rows - return, but don't advance the loop. At the beginning of the
                    /// next call to merge() the same next_cursor will be processed once more and
                    /// the next output row will be created from it.
                    if (graphite_rollup_merged_data.hasEnoughRows())
                        return Status(graphite_rollup_merged_data.pull());
                }

                /// At this point previous row has been fully processed, so we can advance the loop
                /// (substitute current_* values for next_*, advance the cursor).

                startNextGroup(current, next_rule);

                current_time_rounded = next_time_rounded;
            }

            current_time = next_row_time;
        }

        /// Within all rows with same key, we should leave only one row with maximum version;
        /// and for rows with same maximum version - only last row.
        if (is_new_key
            || current->all_columns[columns_definition.version_column_num]->compareAt(
                current->getRow(), current_subgroup_newest_row.row_num,
                *(*current_subgroup_newest_row.all_columns)[columns_definition.version_column_num],
                /* nan_direction_hint = */ 1) >= 0)
        {
            current_subgroup_newest_row.set(current, sources[current.impl->order].chunk);

            /// Small hack: group and subgroups have the same path, so we can set current_group_path here instead of startNextGroup
            /// But since we keep in memory current_subgroup_newest_row's block, we could use string_view for current_group_path and don't
            ///  make deep copy of the path.
            current_group_path = next_path;
        }

        if (!current->isLast())
        {
            queue.next();
        }
        else
        {
            /// We get the next block from the appropriate source, if there is one.
            queue.removeTop();
            return Status(current.impl->order);
        }
    }

    /// Write result row for the last group.
    if (graphite_rollup_merged_data.wasGroupStarted())
    {
        accumulateRow(current_subgroup_newest_row);
        finishCurrentGroup();
    }

    return Status(graphite_rollup_merged_data.pull(), true);
}

void GraphiteRollupSortedAlgorithm::startNextGroup(SortCursor & cursor, Graphite::RollupRule next_rule)
{
    graphite_rollup_merged_data.startNextGroup(cursor->all_columns, cursor->getRow(), next_rule, columns_definition);
}

void GraphiteRollupSortedAlgorithm::finishCurrentGroup()
{
    graphite_rollup_merged_data.insertRow(current_time_rounded, current_subgroup_newest_row, columns_definition);
}

void GraphiteRollupSortedAlgorithm::accumulateRow(RowRef & row)
{
    graphite_rollup_merged_data.accumulateRow(row, columns_definition);
}

void GraphiteRollupSortedAlgorithm::GraphiteRollupMergedData::startNextGroup(
    const ColumnRawPtrs & raw_columns, size_t row,
    Graphite::RollupRule next_rule, ColumnsDefinition & def)
{
    const Graphite::AggregationPattern * aggregation_pattern = std::get<1>(next_rule);

    /// Copy unmodified column values (including path column).
    for (size_t j : def.unmodified_column_numbers)
        columns[j]->insertFrom(*raw_columns[j], row);

    if (aggregation_pattern)
    {
        aggregation_pattern->function->create(place_for_aggregate_state.data());
        aggregate_state_created = true;
    }

    current_rule = next_rule;
    was_group_started = true;
}

void GraphiteRollupSortedAlgorithm::GraphiteRollupMergedData::insertRow(
    time_t time, RowRef & row, ColumnsDefinition & def)
{
    /// Insert calculated values of the columns `time`, `value`, `version`.
    columns[def.time_column_num]->insert(time);
    auto & row_ref_version_column = (*row.all_columns)[def.version_column_num];
    columns[def.version_column_num]->insertFrom(*row_ref_version_column, row.row_num);

    auto & value_column = columns[def.value_column_num];
    const Graphite::AggregationPattern * aggregation_pattern = std::get<1>(current_rule);
    if (aggregate_state_created && aggregation_pattern)
    {
        aggregation_pattern->function->insertResultInto(place_for_aggregate_state.data(), *value_column, nullptr);
        aggregation_pattern->function->destroy(place_for_aggregate_state.data());
        aggregate_state_created = false;
    }
    else
        value_column->insertFrom(*(*row.all_columns)[def.value_column_num], row.row_num);

    ++total_merged_rows;
    ++merged_rows;
    /// TODO: sum_blocks_granularity += block_size;

    was_group_started = false;
}

void GraphiteRollupSortedAlgorithm::GraphiteRollupMergedData::accumulateRow(RowRef & row, ColumnsDefinition & def)
{
    const Graphite::AggregationPattern * aggregation_pattern = std::get<1>(current_rule);
    if (aggregate_state_created && aggregation_pattern)
    {
        auto & column = (*row.all_columns)[def.value_column_num];
        aggregation_pattern->function->add(place_for_aggregate_state.data(), &column, row.row_num, nullptr);
    }
}

GraphiteRollupSortedAlgorithm::GraphiteRollupMergedData::~GraphiteRollupMergedData()
{
    const Graphite::AggregationPattern * aggregation_pattern = std::get<1>(current_rule);
    if (aggregate_state_created && aggregation_pattern)
        aggregation_pattern->function->destroy(place_for_aggregate_state.data());
}

}
