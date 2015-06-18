PostgreSQL Financial Extension
==============================
[![PGXN version](https://badge.fury.io/pg/financial.svg)](https://badge.fury.io/pg/financial)
[![Build Status](https://travis-ci.org/intgr/pg_financial.svg?branch=master)](https://travis-ci.org/intgr/pg_financial)

This is a PostgreSQL extension for financial calculations.

Functions provided:

* `xirr(amounts, dates [, guess])` - Irregular Internal Rate of Return.
  Aggregate function, much like XIRR in spreadsheet programs (Excel,
  LibreOffice, etc).


Installation
------------

To build and install this extension, simply run:

    % make
    % sudo make install

Then, to activate this extension in your database, run the SQL:

    CREATE EXTENSION financial;

If you run into problems with building, see [PostgreSQL wiki for
troubleshooting](https://wiki.postgresql.org/wiki/Extension_build_troubleshooting)


`xirr` aggregate function
-------------------------

The basic form of the XIRR function is: `xirr(amounts, dates [, guess])`

Since XIRR is fundamentally an imprecise function, `amounts` is of type float8
(double precision). `dates` is timestamp with time zone.

For example, if table `transaction` has columns `amount` and `time`, do this:

    db=# SELECT xirr(amount, time ORDER BY time) FROM transaction;
            xirr        
    --------------------
     0.0176201237088334

The `guess` argument (also float8) is an optional initial guess. When omitted,
the function will use annualized return as the guess, which is usually
reliable. Excel and LibreOffice, however, use a guess of 0.1 by default:

    SELECT xirr(amount, time, 0.1 ORDER BY time) FROM transaction;

Like any aggregate function, you can use `xirr` with GROUP BY or as a window
function, e.g:

    SELECT portfolio, xirr(amount, time ORDER BY time)
        FROM transaction GROUP BY portfolio;

    SELECT xirr(amount, time) OVER (ORDER BY time)
        FROM transaction;

There are situations where XIRR (Newton's method) fails to arrive at a result.
In these cases, the function returns NULL. Sometimes providing a better guess
helps, but some inputs are simply indeterminate.

Because XIRR needs to do multiple passes over input data, all inputs to the
aggregate function are held in memory (16 bytes per row). Beware that this can
cause the server to run out of memory with extremely large data sets.

Changelog
---------

1.0.1 (2015-06-18)

* Fix read of uninitialized data
* Implemented continuous integration via Travis-CI
* Minor tweaks to documentation and code

1.0.0 (2014-09-18)

* First stable release, fixes two Makefile issues

0.0.2 (2013-06-05)

* First release, contains `xirr` aggregate function

Copyright and License
---------------------

Copyright (c) 2013-2015 Marti Raudsepp <marti@juffo.org>

pg\_financial and all related files are available under [The PostgreSQL
License](http://www.opensource.org/licenses/PostgreSQL). See LICENSE file for
details.

