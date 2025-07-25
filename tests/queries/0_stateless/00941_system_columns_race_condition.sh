#!/usr/bin/env bash
# Tags: race

# Test fix for issue #5066

CURDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=../shell_config.sh
. "$CURDIR"/../shell_config.sh

set -e

$CLICKHOUSE_CLIENT -q "DROP TABLE IF EXISTS alter_table"
$CLICKHOUSE_CLIENT -q "CREATE TABLE alter_table (a UInt8, b Int16, c Float32, d String, e Array(UInt8), f Nullable(UUID), g Tuple(UInt8, UInt16)) ENGINE = MergeTree ORDER BY a"

function thread1()
{
    local TIMELIMIT=$((SECONDS+$1))
    while [ $SECONDS -lt "$TIMELIMIT" ]
    do
        # NOTE: database = $CLICKHOUSE_DATABASE is unwanted
        $CLICKHOUSE_CLIENT --query "SELECT name FROM system.columns UNION ALL SELECT name FROM system.columns FORMAT Null";
    done
}

function thread2()
{
    local TIMELIMIT=$((SECONDS+$1))
    while [ $SECONDS -lt "$TIMELIMIT" ]
    do
        $CLICKHOUSE_CLIENT -n --query "ALTER TABLE alter_table ADD COLUMN h String; ALTER TABLE alter_table MODIFY COLUMN h UInt64; ALTER TABLE alter_table DROP COLUMN h;";
    done
}

TIMEOUT=15

thread1 $TIMEOUT 2> /dev/null &
thread1 $TIMEOUT 2> /dev/null &
thread1 $TIMEOUT 2> /dev/null &
thread1 $TIMEOUT 2> /dev/null &
thread2 $TIMEOUT 2> /dev/null &
thread2 $TIMEOUT 2> /dev/null &
thread2 $TIMEOUT 2> /dev/null &
thread2 $TIMEOUT 2> /dev/null &
thread1 $TIMEOUT 2> /dev/null &
thread1 $TIMEOUT 2> /dev/null &
thread1 $TIMEOUT 2> /dev/null &
thread1 $TIMEOUT 2> /dev/null &
thread2 $TIMEOUT 2> /dev/null &
thread2 $TIMEOUT 2> /dev/null &
thread2 $TIMEOUT 2> /dev/null &
thread2 $TIMEOUT 2> /dev/null &

wait

$CLICKHOUSE_CLIENT -q "DROP TABLE alter_table"
