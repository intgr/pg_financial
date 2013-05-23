CREATE EXTENSION financial;

SET extra_float_digits = -4;
-- SET client_min_messages = 'debug';

SELECT xirr(amt, ts) FROM (VALUES(-100::float, '2013-01-01'::timestamptz), (110, '2013-02-01')) x(amt, ts);
SELECT xirr(amt, ts) FROM (VALUES(-100::float, '2013-01-01'::timestamptz), ('Infinity', '2013-02-01')) x(amt, ts);
SELECT xirr(-100, '2013-01-01');
SELECT xirr(100, '2013-01-01');
SELECT xirr(NULL, '2013-01-01');
SELECT xirr(100, NULL);

CREATE TEMPORARY TABLE transactions AS
    SELECT -10.0::float8 amt, generate_series(timestamptz '2001-01-01', '2012-01-01', '1 day') ts
    UNION
    SELECT 12.0::float8, generate_series(timestamptz '2001-02-01', '2012-02-01', '1 day')
;

SELECT xirr(amt, ts ORDER BY ts) FROM transactions;

SELECT xirr(amt, ts) FROM (SELECT sum(amt) as amt, ts FROM transactions GROUP BY ts ORDER BY ts) subq;

SELECT xirr(amt, ts) FROM (SELECT amt, ts FROM transactions ORDER BY ts, random()) subq;

-- Excel's default guess of 0.1 would fail here, but hey, we're smarter than that
SELECT xirr(amt, ts, null) FROM (VALUES(-100::float, '2013-01-01'::timestamptz), (20, '2014-01-01')) x(amt, ts);
SELECT xirr(amt, ts, 0.1) FROM (VALUES(-100::float, '2013-01-01'::timestamptz), (20, '2014-01-01')) x(amt, ts);
SELECT xirr(amt, ts, -0.9999) FROM (VALUES(-100::float, '2013-01-01'::timestamptz), (20, '2014-01-01')) x(amt, ts);

-- Window function
SELECT xirr(amt, ts) OVER (ORDER BY ts) FROM (VALUES(-100::float, '2013-01-01'::timestamptz), (10, '2013-02-01'), (110, '2013-03-01')) x(amt, ts);
