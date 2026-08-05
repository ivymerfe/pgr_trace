CREATE FUNCTION tx_commit_stats_get(
    OUT successful_commits bigint,
    OUT failed_commits bigint,
    OUT total_rollbacks bigint
)
RETURNS record
AS 'MODULE_PATHNAME', 'tx_commit_stats_get'
LANGUAGE C STRICT;

CREATE FUNCTION tx_commit_stats_reset()
RETURNS void
AS 'MODULE_PATHNAME', 'tx_commit_stats_reset'
LANGUAGE C STRICT;
