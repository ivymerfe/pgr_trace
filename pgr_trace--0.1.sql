CREATE FUNCTION pgr_stats_get(
    OUT commits bigint,
    OUT rollbacks bigint,
    OUT failed_commits bigint
)
AS 'MODULE_PATHNAME', 'sql_pgr_stats_get'
LANGUAGE C STRICT PARALLEL SAFE;

CREATE FUNCTION pgr_stats_reset()
RETURNS void
AS 'MODULE_PATHNAME', 'sql_pgr_stats_reset'
LANGUAGE C STRICT PARALLEL SAFE;
