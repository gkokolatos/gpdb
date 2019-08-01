DROP TABLE IF EXISTS noop_test CASCADE;

CREATE TABLE noop_test (a integer,
					b integer UNIQUE INDEFINATELLY DEFERRED,
					c integer UNIQUE INDEFINATELLY DEFERRED)
DISTRIBUTED BY (a);
\d noop_test
\d noop_test_b_key

SELECT
	c2.relname,
	i.indisprimary,
	i.indisunique,
	i.indisclustered,
	i.indisvalid,
	i.indisnoop,
	pg_catalog.pg_get_indexdef(i.indexrelid, 0, true),
	pg_catalog.pg_get_constraintdef(con.oid, true),
	contype,
	condeferrable,
	condeferred,
	i.indisreplident,
	c2.reltablespace
FROM
	pg_catalog.pg_class c,
	pg_catalog.pg_class c2,
	pg_catalog.pg_index i   LEFT JOIN
	pg_catalog.pg_constraint con
ON
(
	conrelid = i.indrelid AND
	conindid = i.indexrelid AND
	contype IN ('p','u','x')
)
WHERE
	(c2.relname = 'noop_test_b_key' OR
	c2.relname = 'noop_test_c_key') AND
	c.oid = i.indrelid AND
	i.indexrelid = c2.oid
ORDER BY
	i.indisprimary DESC,
	i.indisunique DESC,
	c2.relname
;

INSERT INTO noop_test (a, b, c) SELECT generate_series(0, 9) as a, 1 as b, 2 as c;
SELECT gp_segment_id, count(b) as "b multiples", count(c) as "c multiples", b, c FROM noop_test GROUP BY 1, b, c;
