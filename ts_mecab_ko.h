/*
 * ts_mecab_ko.h
 * License : BSD
 * This file is modified from textsearch_ja.h
 * Change Contents
 *  - code refactoring
 *  - customizing for mecab-ko-dic data
 * Copyright (c) 2014, Ioseph Kim
 */

#ifndef TS_MECAB_KO_H
#define TS_MECAB_KO_H

#include "lib/stringinfo.h"

typedef void (*append_t)(StringInfo dst, const unsigned char *src, int srclen);

#define StringInfoTail(str, at) \
	((unsigned char *) ((str)->data + (str)->len - (at)))

#define uchar_mblen(ustr)				pg_mblen((const char *) (ustr))
#define uchar_strlen(ustr)				strlen((const char *) (ustr))
#define appendMBString(dst, ustr, len)	appendBinaryStringInfo((dst), (const char *) (ustr), (len))

#endif /* TS_MECAB_KO_H */
