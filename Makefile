EXTENSION = textsearch_ko        # the extensions name
DATA = textsearch_ko--1.0.sql  # script files to install
REGRESS = textsearch_ko_test # our test script file (without extension)
MODULE_big = ts_mecab_ko
relocatable = true

OBJS = \
	$(WIN32RES) \
	ts_mecab_ko.o

PGFILEDESC = "textsearch_ko - textsearch for korean"

MECAB_HEADER = $(shell mecab-config --inc-dir)
MECAB_LIBS = $(shell mecab-config --libs)

PG_CFLAGS += -I$(MECAB_HEADER)
PG_CPPFLAGS += -I$(MECAB_HEADER)
SHLIB_LINK += $(MECAB_LIBS)

# postgres build stuff
ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/textsearch_ko
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
