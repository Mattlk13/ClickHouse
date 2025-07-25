-- Tags: stateful, no-random-settings, no-parallel
-- no-parallel: Heavy query

SET max_memory_usage = '10G';
SELECT sum(cityHash64(*)) FROM test.hits SETTINGS max_threads=40;

-- We had a bug which lead to additional compressed data read. test.hits compressed size is about 1.2Gb, but we read more then 3Gb.
-- Small additional reads still possible, so we compare with about 1.5Gb.
SYSTEM FLUSH LOGS query_log;

SELECT ProfileEvents['ReadBufferFromFileDescriptorReadBytes'] < 1500000000 from system.query_log where query = 'SELECT sum(cityHash64(*)) FROM test.hits SETTINGS max_threads=40;' and current_database = currentDatabase() and type = 'QueryFinish';
