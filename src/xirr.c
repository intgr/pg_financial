// vim: set noet sw=4 ts=4 :
/*-------------------------------------------------------------------------
 *
 * xirr.c
 *	  Irregular Internal Rate of Return
 *
 * Copyright (c) 2013-2015 Marti Raudsepp <marti@juffo.org>
 *
 *-------------------------------------------------------------------------
 */

#include <math.h>

#include "postgres.h"
#include "miscadmin.h"
#include "utils/timestamp.h"

/**** Declarations */

PG_MODULE_MAGIC;

typedef struct XirrItem
{
	double		amount;
	TimestampTz time;
} XirrItem;

typedef struct XirrState
{
	int			alen;			/* allocated length of array */
	int			nelems;			/* number of elements filled in array */
	double		guess;			/* initial guess for Newton's method */
	XirrItem	array[0];		/* array of values */
} XirrState;


extern Datum xirr_tstz_transfn(PG_FUNCTION_ARGS);
extern Datum xirr_tstz_finalfn(PG_FUNCTION_ARGS);
static double calculate_xirr(const XirrState *state, double guess);
static double calculate_annualized_return(const XirrState *state);

/**** Implementation */

/*
 * Aggregate state function. Accumulates amount+timestamp values in XirrState.
 */
PG_FUNCTION_INFO_V1(xirr_tstz_transfn);

Datum
xirr_tstz_transfn(PG_FUNCTION_ARGS)
{
	MemoryContext oldcontext;
	MemoryContext aggcontext;
	XirrState  *state;
	double		amount;
	TimestampTz	time;

	if (PG_ARGISNULL(1))
		elog(ERROR, "xirr amount input cannot be NULL");
	if (PG_ARGISNULL(2))
		elog(ERROR, "xirr timestamp input cannot be NULL");

	if (PG_ARGISNULL(0))
	{
		const int initlen = 64;

		if (!AggCheckCallContext(fcinfo, &aggcontext))
		{
			/* cannot be called directly because of internal-type argument */
			elog(ERROR, "xirr_tstz_transfn called in non-aggregate context");
		}
		oldcontext = MemoryContextSwitchTo(aggcontext);

		state = palloc(sizeof(XirrState) + initlen * sizeof(XirrItem));
		state->alen = initlen;
		state->nelems = 0;

		/* Optional "guess" argument */
		if (PG_NARGS() >= 4 && !PG_ARGISNULL(3))
			state->guess = PG_GETARG_FLOAT8(3);
		else
			state->guess = NAN;

		MemoryContextSwitchTo(oldcontext);
	}
	else
	{
		state = (XirrState *) PG_GETARG_POINTER(0);
	}

	amount = PG_GETARG_FLOAT8(1);
	if (amount == 0.0)
		PG_RETURN_POINTER(state);

	time = PG_GETARG_TIMESTAMPTZ(2);

	/* Coalesce payment with the previous one if it's on the same time */
	if (state->nelems > 0 && time == state->array[state->nelems-1].time)
	{
		state->array[state->nelems-1].amount += amount;
		PG_RETURN_POINTER(state);
	}

	/* Have to append a new record */
	if (state->nelems >= state->alen)
	{
		if (!AggCheckCallContext(fcinfo, &aggcontext))
		{
			/* cannot be called directly because of internal-type argument */
			elog(ERROR, "xirr_tstz_transfn called in non-aggregate context");
		}
		oldcontext = MemoryContextSwitchTo(aggcontext);

		state->alen *= 2;
		state = repalloc(state, sizeof(XirrState) + state->alen * sizeof(XirrItem));

		MemoryContextSwitchTo(oldcontext);
	}

	state->array[state->nelems].amount = amount;
	state->array[state->nelems].time = time;
	state->nelems++;

	PG_RETURN_POINTER(state);
}

/*
 * Aggregate finalize function. Takes the accumulated array and actually
 * calculates the result.
 *
 * May not change XirrState, otherwise it breaks window functions.
 */
PG_FUNCTION_INFO_V1(xirr_tstz_finalfn);

Datum
xirr_tstz_finalfn(PG_FUNCTION_ARGS)
{
	const XirrState *state;
	double		ret;
	double		guess;

	/* no input rows */
	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();

	state = (XirrState *) PG_GETARG_POINTER(0);

	if (state->nelems < 2)
		PG_RETURN_NULL();

	/* Guess not provided as argument? */
	if (isnan(state->guess))
		guess = calculate_annualized_return(state);
	else
		guess = state->guess;

	elog(DEBUG1, "Calculating XIRR over %d records, %ld MB memory, guess=%g",
		 state->nelems, (long)((state->nelems*sizeof(XirrItem))/(1024*1024)),
		 guess);

	ret = calculate_xirr(state, guess);

	if (isnan(ret))
		PG_RETURN_NULL();
	else
		PG_RETURN_FLOAT8(ret);
}

/*
 * Internal function to calculate XIRR of a XirrState.
 *
 * Constants are same as used in LibreOffice.
 */
#define MAX_LOOPS			50
#define MAX_EPSILON			1e-10
#define XIRR_DAYS_PER_YEAR	365.0

#ifdef HAVE_INT64_TIMESTAMP
#	define TIME_PER_YEAR 	(USECS_PER_DAY * XIRR_DAYS_PER_YEAR)
#else
#	define TIME_PER_YEAR 	(SECS_PER_DAY * XIRR_DAYS_PER_YEAR)
#endif

static double
calculate_xirr(const XirrState *state, double guess)
{
	int 		i, j;
	TimestampTz time0 = state->array[0].time;

	/* Newton's method */
	for (j = 0; j < MAX_LOOPS; j++)
	{
		double deriv = 0.0;
		double result = state->array[0].amount;
		double r = guess + 1.0;
		double epsilon, new_rate;

		for (i = 1; i < state->nelems; i++)
		{
			/* What fraction of a year has passed? */
			double years = (state->array[i].time - time0) / TIME_PER_YEAR;
			double val = state->array[i].amount;
			double exp = pow(r, years);

			result += val / exp;
			deriv -= years * val / (exp * r); /* (exp * r) = pow(r, years + 1) */
		}

		new_rate = guess - (result / deriv);
		epsilon = fabs(new_rate - guess);

		elog(DEBUG1, "Iteration %2d rate %-8g [epsilon %-8g]", j, new_rate, epsilon);

		/* It's not getting any better by adding numbers to infinity */
		if (!isfinite(new_rate))
			return NAN;

		if (epsilon <= MAX_EPSILON || fabs(result) < MAX_EPSILON)
			return new_rate;

		/* Try another guess, hopefully we're closer now. */
		guess = new_rate;

		CHECK_FOR_INTERRUPTS();
	}

	/* Didn't converge */
	return NAN;
}

/*
 * Calculate annualized return, used as the initial guess for Newton's method.
 *
 * While this can be calculated incrementally in transfn -- unlike XIRR -- it
 * probably isn't worth it, since we already have this nice array anyway.
 *
 * ((1+sum(amount)/-sum(case when amount<0 then amount end))^(365.0/(max(ts::date)-min(ts::date))))-1
 */
static double
calculate_annualized_return(const XirrState *state)
{
	int			i;
	double		debit = 0.0;
	double		endvalue = 0.0;
	double		power;
	TimestampTz	mintime, maxtime;

	/* Try to be clever, input is most likely sorted by time. */
	mintime = state->array[0].time,
	maxtime = state->array[state->nelems-1].time;

	for (i = 0; i < state->nelems; i++)
	{
		double		val = state->array[i].amount;
		TimestampTz	time = state->array[i].time;

		endvalue += val;
		if (val < 0.0)
			debit -= val;

		if (time > maxtime)
			maxtime = time;
		else if (time < mintime)
			mintime = time;
	}

	power = TIME_PER_YEAR/(maxtime-mintime);

	/* +/- swapped */
	if (state->array[0].amount > 0)
		power = -power;
	return pow(1 + endvalue/debit, power) - 1;
}
