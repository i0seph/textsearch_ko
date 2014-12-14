#!/bin/sh

export PATH=/data/pgsql/9.3/bin:/data/mecab-ko/bin:$PATH
C_INCLUDE_PATH=/data/mecab-ko/include LIBRARY_PATH=/data/mecab-ko/lib make USE_PGXS=1 install
