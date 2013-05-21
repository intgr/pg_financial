CREATE EXTENSION financial;

-- SET client_min_messages = 'debug';
SELECT xirr(amt, ts) FROM (VALUES(-100, '2013-01-01'::timestamptz), (110, '2013-02-01')) x(amt, ts);
