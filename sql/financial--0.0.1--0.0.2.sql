-- xirr(amount, time, guess)
CREATE FUNCTION xirr_tstz_transfn (internal, float8, timestamptz, float8)
RETURNS internal IMMUTABLE LANGUAGE C AS 'financial';

CREATE AGGREGATE xirr (float8, timestamptz, float8) (
    SFUNC = xirr_tstz_transfn,
    STYPE = internal,
    FINALFUNC = xirr_tstz_finalfn
);

