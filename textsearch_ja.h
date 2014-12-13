/*
 * textsearch_ja.h
 */
#ifndef TEXTSEARCH_JA_H
#define TEXTSEARCH_JA_H

#include "lib/stringinfo.h"

typedef struct IgnorableWord
{
	int			len;
	const char *word;
} IgnorableWord;

typedef void (*append_t)(StringInfo dst, const unsigned char *src, int srclen);

extern void normalize_utf8(StringInfo dst, const char *src, size_t srclen, append_t append);
extern void normalize_eucjp(StringInfo dst, const char *src, size_t srclen, append_t append);
extern char *lexize_utf8(const char *str, size_t len);
extern char *lexize_eucjp(const char *str, size_t len);
extern void hiragana_utf8(StringInfo dst, const char *src, size_t srclen);
extern void hiragana_eucjp(StringInfo dst, const char *src, size_t srclen);
extern void katakana_utf8(StringInfo dst, const char *src, size_t srclen);
extern void katakana_eucjp(StringInfo dst, const char *src, size_t srclen);

extern const IgnorableWord	IGNORE_UTF8[];
extern const IgnorableWord	IGNORE_EUCJP[];

#define StringInfoTail(str, at) \
	((unsigned char *) ((str)->data + (str)->len - (at)))

#define uchar_mblen(ustr)				pg_mblen((const char *) (ustr))
#define uchar_strlen(ustr)				strlen((const char *) (ustr))
#define appendMBString(dst, ustr, len)	appendBinaryStringInfo((dst), (const char *) (ustr), (len))

#endif /* TEXTSEARCH_JA_H */
