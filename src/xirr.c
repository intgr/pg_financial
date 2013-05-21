// vim: set noet sw=4 ts=4 :
/*-------------------------------------------------------------------------
 *
 * xirr.c
 *	  Irregular Internal Rate of Return
 *
 * Copyright (c) 2003-2013, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/array_userfuncs.c
 *
 *-------------------------------------------------------------------------
 */

#define _ISOC99_SOURCE
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
	XirrItem	array[0];		/* array of values */
} XirrState;


extern Datum xirr_tstz_transfn(PG_FUNCTION_ARGS);
extern Datum xirr_tstz_finalfn(PG_FUNCTION_ARGS);
static double calculate_xirr(XirrState *state);

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

	/* FIXME */
	Assert(!PG_ARGISNULL(1));
	Assert(!PG_ARGISNULL(2));

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

		MemoryContextSwitchTo(oldcontext);
	}
	else
	{
		state = (XirrState *) PG_GETARG_POINTER(0);

		/* enlarge array if needed */
		if(state->nelems >= state->alen)
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
	}

	amount = PG_GETARG_FLOAT8(1);
	time =  PG_GETARG_TIMESTAMPTZ(2);

	state->array[state->nelems].amount = amount;
	state->array[state->nelems].time = time;
	state->nelems++;

	PG_RETURN_POINTER(state);
}

/*
 * Aggregate finalize function. Takes the accumulated array and actually
 * calculates the result.
 */
PG_FUNCTION_INFO_V1(xirr_tstz_finalfn);

Datum
xirr_tstz_finalfn(PG_FUNCTION_ARGS)
{
	XirrState  *state;
	double		ret;

	/* no input rows */
	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();

	state = (XirrState *) PG_GETARG_POINTER(0);

	elog(DEBUG1, "Calculating XIRR over %d records, %ld MB memory",
		 state->nelems, (state->nelems*sizeof(XirrItem))/(1024*1024));

	if (state->nelems < 1)
		PG_RETURN_NULL();

	ret = calculate_xirr(state);

	if(isnan(ret))
		PG_RETURN_NULL();
	else
		PG_RETURN_FLOAT8(ret);
}

/*
 * Internal function to calculate XIRR of a XirrState.
 *
 * Constants are same as used in LibreOffice.
 */
#define MAX_LOOPS 50
#define MAX_EPSILON 1e-10
#define INITIAL_GUESS 0.1

static double
calculate_xirr(XirrState *state)
{
	int 		i, j = 0;
	TimestampTz time0 = state->array[0].time;
	double		old_rate = INITIAL_GUESS,
				new_rate;

	/* Newton's method */
	for(j = 0; j < MAX_LOOPS; j++)
	{
		double 		deriv = 0.0;
		double 		result = state->array[0].amount;
		double 		r = old_rate + 1.0;
		double		epsilon;

		for(i = 1; i < state->nelems; i++)
		{
			/* What fraction of a year has passed? */
			double years = (state->array[i].time - time0) / (USECS_PER_DAY * 365.0 /*DAYS_PER_YEAR*/);
			double val = state->array[i].amount;

			result += val / pow(r, years);
			deriv -= years * val / pow(r, years + 1.0);
		}

		new_rate = old_rate - (result / deriv);
		epsilon = fabs(new_rate - old_rate);

		elog(DEBUG1, "Iteration %2d rate %8g [epsilon %8g]", j, new_rate, epsilon);

		if(!isfinite(result) || epsilon <= MAX_EPSILON || fabs(result) < MAX_EPSILON)
			return new_rate;

		old_rate = new_rate;

		CHECK_FOR_INTERRUPTS();
	}

	/* Didn't converge */
	return NAN;
}
