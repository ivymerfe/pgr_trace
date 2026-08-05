CREATE FUNCTION tx_commit_stats_get(
    OUT successful_commits bigint,
    OUT failed_commits bigint,
    OUT rollbacks bigint
)
AS 'MODULE_PATHNAME', 'tx_commit_stats_get'
LANGUAGE C STRICT PARALLEL SAFE;

CREATE FUNCTION tx_commit_stats_reset()
RETURNS void
AS 'MODULE_PATHNAME', 'tx_commit_stats_reset'
LANGUAGE C STRICT PARALLEL SAFE;
