EXTENSION    = financial
EXTVERSION   = $(shell grep default_version $(EXTENSION).control | \
               sed -e "s/default_version[[:space:]]*=[[:space:]]*'\([^']*\)'/\1/")

DATA         = $(filter-out $(wildcard sql/*--*.sql),$(wildcard sql/*.sql))
TESTS        = $(wildcard test/sql/*.sql)
REGRESS      = $(patsubst test/sql/%.sql,%,$(TESTS))
REGRESS_OPTS = --inputdir=test
#DOCS         = README.md
MODULE_big   = financial
OBJS         = $(patsubst %.c,%.o,$(wildcard src/*.c))
PG_CONFIG    = pg_config
PG91         = $(shell $(PG_CONFIG) --version | grep -qE " 8\.| 9\.0" && echo no || echo yes)

ifeq ($(PG91),yes)
all:

sql/$(EXTENSION)--$(EXTVERSION).sql: sql/$(EXTENSION).sql
	cp $< $@

DATA_built = sql/$(EXTENSION)--$(EXTVERSION).sql
DATA = $(wildcard sql/*--*.sql)
endif

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

install: install-readme

install-readme:
ifdef docdir
	$(INSTALL_DATA) $(srcdir)/README.md '$(DESTDIR)$(docdir)/$(docmoduledir)/financial.md'
endif # docdir
