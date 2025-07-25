#!/usr/bin/env bash
# Tags: race, zookeeper

CURDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=../shell_config.sh
. "$CURDIR"/../shell_config.sh

set -e

function thread1()
{
    local TIMELIMIT=$((SECONDS+TIMEOUT))
    while [ $SECONDS -lt "$TIMELIMIT" ]; do
        $CLICKHOUSE_CLIENT -q "INSERT INTO concurrent_optimize_table SELECT rand(1), rand(2), 1 / rand(3), toString(rand(4)), [rand(5), rand(6)], rand(7) % 2 ? NULL : generateUUIDv4(), (rand(8), rand(9)) FROM numbers(10000)";
    done
}


function thread2()
{
    local TIMELIMIT=$((SECONDS+TIMEOUT))
    while [ $SECONDS -lt "$TIMELIMIT" ]; do
        $CLICKHOUSE_CLIENT -q "OPTIMIZE TABLE concurrent_optimize_table FINAL";
        sleep 0.$RANDOM;
    done
}

function thread3()
{
    local TIMELIMIT=$((SECONDS+TIMEOUT))
    while [ $SECONDS -lt "$TIMELIMIT" ]; do
        $CLICKHOUSE_CLIENT -n -q "DROP TABLE IF EXISTS concurrent_optimize_table;
            CREATE TABLE concurrent_optimize_table (a UInt8, b Int16, c Float32, d String, e Array(UInt8), f Nullable(UUID), g Tuple(UInt8, UInt16))
            ENGINE = ReplicatedMergeTree('/clickhouse/tables/$CLICKHOUSE_TEST_ZOOKEEPER_PREFIX/concurrent_optimize_table', '1') ORDER BY a PARTITION BY b % 10
            SETTINGS old_parts_lifetime = 1, cleanup_delay_period = 0, cleanup_delay_period_random_add = 0, cleanup_thread_preferred_points_per_iteration=0;";
        sleep 0.$RANDOM;
        sleep 0.$RANDOM;
        sleep 0.$RANDOM;
    done
}


TIMEOUT=15

thread1 2> /dev/null &
thread2 2> /dev/null &
thread3 2> /dev/null &

thread1 2> /dev/null &
thread2 2> /dev/null &
thread3 2> /dev/null &

thread1 2> /dev/null &
thread2 2> /dev/null &
thread3 2> /dev/null &

thread1 2> /dev/null &
thread2 2> /dev/null &
thread3 2> /dev/null &

wait

$CLICKHOUSE_CLIENT --query "DROP TABLE IF EXISTS concurrent_optimize_table"
