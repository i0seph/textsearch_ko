/*
 * normalize_utf8.c
 */
#include "postgres.h"
#include "mb/pg_wchar.h"

#include "textsearch_ja.h"

#define WIDEN(c1, c2)		(((unsigned char) (c1) << 8) | (unsigned char) (c2))

typedef struct Map
{
	unsigned short	from;
	unsigned char	to[4];
} Map;

/*
 * 3byte 文字の後半 2byte をキーとして検索する.
 */
static const Map UTF8_MAP[] =
{
	{ 0xbc81, { 0x21 } },
	{ 0xbc83, { 0x23 } },
	{ 0xbc84, { 0x24 } },
	{ 0xbc85, { 0x25 } },
	{ 0xbc86, { 0x26 } },
	{ 0xbc88, { 0x28 } },
	{ 0xbc89, { 0x29 } },
	{ 0xbc8a, { 0x2a } },
	{ 0xbc8b, { 0x2b } },
	{ 0xbc8c, { 0x2c } },
	{ 0xbc8d, { 0x2d } },
	{ 0xbc8e, { 0x2e } },
	{ 0xbc8f, { 0x2f } },
	{ 0xbc90, { 0x30 } },
	{ 0xbc91, { 0x31 } },
	{ 0xbc92, { 0x32 } },
	{ 0xbc93, { 0x33 } },
	{ 0xbc94, { 0x34 } },
	{ 0xbc95, { 0x35 } },
	{ 0xbc96, { 0x36 } },
	{ 0xbc97, { 0x37 } },
	{ 0xbc98, { 0x38 } },
	{ 0xbc99, { 0x39 } },
	{ 0xbc9a, { 0x3a } },
	{ 0xbc9b, { 0x3b } },
	{ 0xbc9c, { 0x3c } },
	{ 0xbc9d, { 0x3d } },
	{ 0xbc9e, { 0x3e } },
	{ 0xbc9f, { 0x3f } },
	{ 0xbca0, { 0x40 } },
	{ 0xbca1, { 0x41 } },
	{ 0xbca2, { 0x42 } },
	{ 0xbca3, { 0x43 } },
	{ 0xbca4, { 0x44 } },
	{ 0xbca5, { 0x45 } },
	{ 0xbca6, { 0x46 } },
	{ 0xbca7, { 0x47 } },
	{ 0xbca8, { 0x48 } },
	{ 0xbca9, { 0x49 } },
	{ 0xbcaa, { 0x4a } },
	{ 0xbcab, { 0x4b } },
	{ 0xbcac, { 0x4c } },
	{ 0xbcad, { 0x4d } },
	{ 0xbcae, { 0x4e } },
	{ 0xbcaf, { 0x4f } },
	{ 0xbcb0, { 0x50 } },
	{ 0xbcb1, { 0x51 } },
	{ 0xbcb2, { 0x52 } },
	{ 0xbcb3, { 0x53 } },
	{ 0xbcb4, { 0x54 } },
	{ 0xbcb5, { 0x55 } },
	{ 0xbcb6, { 0x56 } },
	{ 0xbcb7, { 0x57 } },
	{ 0xbcb8, { 0x58 } },
	{ 0xbcb9, { 0x59 } },
	{ 0xbcba, { 0x5a } },
	{ 0xbcbb, { 0x5b } },
	{ 0xbcbd, { 0x5d } },
	{ 0xbcbe, { 0x5e } },
	{ 0xbcbf, { 0x5f } },
	{ 0xbd81, { 0x61 } },
	{ 0xbd82, { 0x62 } },
	{ 0xbd83, { 0x63 } },
	{ 0xbd84, { 0x64 } },
	{ 0xbd85, { 0x65 } },
	{ 0xbd86, { 0x66 } },
	{ 0xbd87, { 0x67 } },
	{ 0xbd88, { 0x68 } },
	{ 0xbd89, { 0x69 } },
	{ 0xbd8a, { 0x6a } },
	{ 0xbd8b, { 0x6b } },
	{ 0xbd8c, { 0x6c } },
	{ 0xbd8d, { 0x6d } },
	{ 0xbd8e, { 0x6e } },
	{ 0xbd8f, { 0x6f } },
	{ 0xbd90, { 0x70 } },
	{ 0xbd91, { 0x71 } },
	{ 0xbd92, { 0x72 } },
	{ 0xbd93, { 0x73 } },
	{ 0xbd94, { 0x74 } },
	{ 0xbd95, { 0x75 } },
	{ 0xbd96, { 0x76 } },
	{ 0xbd97, { 0x77 } },
	{ 0xbd98, { 0x78 } },
	{ 0xbd99, { 0x79 } },
	{ 0xbd9a, { 0x7a } },
	{ 0xbd9b, { 0x7b } },
	{ 0xbd9c, { 0x7c } },
	{ 0xbd9d, { 0x7d } },
	{ 0xbda6, { 0xe3, 0x83, 0xb2 } },
	{ 0xbda7, { 0xe3, 0x82, 0xa1 } },
	{ 0xbda8, { 0xe3, 0x82, 0xa3 } },
	{ 0xbda9, { 0xe3, 0x82, 0xa5 } },
	{ 0xbdaa, { 0xe3, 0x82, 0xa7 } },
	{ 0xbdab, { 0xe3, 0x82, 0xa9 } },
	{ 0xbdac, { 0xe3, 0x83, 0xa3 } },
	{ 0xbdad, { 0xe3, 0x83, 0xa5 } },
	{ 0xbdae, { 0xe3, 0x83, 0xa7 } },
	{ 0xbdaf, { 0xe3, 0x83, 0x83 } },
	{ 0xbdb0, { 0xe3, 0x83, 0xbc } },
	{ 0xbdb1, { 0xe3, 0x82, 0xa2 } },
	{ 0xbdb2, { 0xe3, 0x82, 0xa4 } },
	{ 0xbdb3, { 0xe3, 0x82, 0xa6 } },
	{ 0xbdb4, { 0xe3, 0x82, 0xa8 } },
	{ 0xbdb5, { 0xe3, 0x82, 0xaa } },
	{ 0xbdb6, { 0xe3, 0x82, 0xab } },
	{ 0xbdb7, { 0xe3, 0x82, 0xad } },
	{ 0xbdb8, { 0xe3, 0x82, 0xaf } },
	{ 0xbdb9, { 0xe3, 0x82, 0xb1 } },
	{ 0xbdba, { 0xe3, 0x82, 0xb3 } },
	{ 0xbdbb, { 0xe3, 0x82, 0xb5 } },
	{ 0xbdbc, { 0xe3, 0x82, 0xb7 } },
	{ 0xbdbd, { 0xe3, 0x82, 0xb9 } },
	{ 0xbdbe, { 0xe3, 0x82, 0xbb } },
	{ 0xbdbf, { 0xe3, 0x82, 0xbd } },
	{ 0xbe80, { 0xe3, 0x82, 0xbf } },
	{ 0xbe81, { 0xe3, 0x83, 0x81 } },
	{ 0xbe82, { 0xe3, 0x83, 0x84 } },
	{ 0xbe83, { 0xe3, 0x83, 0x86 } },
	{ 0xbe84, { 0xe3, 0x83, 0x88 } },
	{ 0xbe85, { 0xe3, 0x83, 0x8a } },
	{ 0xbe86, { 0xe3, 0x83, 0x8b } },
	{ 0xbe87, { 0xe3, 0x83, 0x8c } },
	{ 0xbe88, { 0xe3, 0x83, 0x8d } },
	{ 0xbe89, { 0xe3, 0x83, 0x8e } },
	{ 0xbe8a, { 0xe3, 0x83, 0x8f } },
	{ 0xbe8b, { 0xe3, 0x83, 0x92 } },
	{ 0xbe8c, { 0xe3, 0x83, 0x95 } },
	{ 0xbe8d, { 0xe3, 0x83, 0x98 } },
	{ 0xbe8e, { 0xe3, 0x83, 0x9b } },
	{ 0xbe8f, { 0xe3, 0x83, 0x9e } },
	{ 0xbe90, { 0xe3, 0x83, 0x9f } },
	{ 0xbe91, { 0xe3, 0x83, 0xa0 } },
	{ 0xbe92, { 0xe3, 0x83, 0xa1 } },
	{ 0xbe93, { 0xe3, 0x83, 0xa2 } },
	{ 0xbe94, { 0xe3, 0x83, 0xa4 } },
	{ 0xbe95, { 0xe3, 0x83, 0xa6 } },
	{ 0xbe96, { 0xe3, 0x83, 0xa8 } },
	{ 0xbe97, { 0xe3, 0x83, 0xa9 } },
	{ 0xbe98, { 0xe3, 0x83, 0xaa } },
	{ 0xbe99, { 0xe3, 0x83, 0xab } },
	{ 0xbe9a, { 0xe3, 0x83, 0xac } },
	{ 0xbe9b, { 0xe3, 0x83, 0xad } },
	{ 0xbe9c, { 0xe3, 0x83, 0xaf } },
	{ 0xbe9d, { 0xe3, 0x83, 0xb3 } },
	{ 0xbe9e, { 0xe3, 0x82, 0x9b } },
	{ 0xbe9f, { 0xe3, 0x82, 0x9c } },
	{ 0xbfa3, { 0x7e } },
	{ 0xbfa5, { 0x5c } },
};

static const Map UTF8_HANKANA2ZENHIRA[] =
{
	{ 0xbda6, { 0343, 0202, 0222 } },
	{ 0xbda7, { 0343, 0201, 0201 } },
	{ 0xbda8, { 0343, 0201, 0203 } },
	{ 0xbda9, { 0343, 0201, 0205 } },
	{ 0xbdaa, { 0343, 0201, 0207 } },
	{ 0xbdab, { 0343, 0201, 0211 } },
	{ 0xbdac, { 0343, 0202, 0203 } },
	{ 0xbdad, { 0343, 0202, 0205 } },
	{ 0xbdae, { 0343, 0202, 0207 } },
	{ 0xbdaf, { 0343, 0201, 0243 } },
	{ 0xbdb1, { 0343, 0201, 0202 } },
	{ 0xbdb2, { 0343, 0201, 0204 } },
	{ 0xbdb3, { 0343, 0201, 0206 } },
	{ 0xbdb4, { 0343, 0201, 0210 } },
	{ 0xbdb5, { 0343, 0201, 0212 } },
	{ 0xbdb6, { 0343, 0201, 0213 } },
	{ 0xbdb7, { 0343, 0201, 0215 } },
	{ 0xbdb8, { 0343, 0201, 0217 } },
	{ 0xbdb9, { 0343, 0201, 0221 } },
	{ 0xbdba, { 0343, 0201, 0223 } },
	{ 0xbdbb, { 0343, 0201, 0225 } },
	{ 0xbdbc, { 0343, 0201, 0227 } },
	{ 0xbdbd, { 0343, 0201, 0231 } },
	{ 0xbdbe, { 0343, 0201, 0233 } },
	{ 0xbdbf, { 0343, 0201, 0235 } },
	{ 0xbe80, { 0343, 0201, 0237 } },
	{ 0xbe81, { 0343, 0201, 0241 } },
	{ 0xbe82, { 0343, 0201, 0244 } },
	{ 0xbe83, { 0343, 0201, 0246 } },
	{ 0xbe84, { 0343, 0201, 0250 } },
	{ 0xbe85, { 0343, 0201, 0252 } },
	{ 0xbe86, { 0343, 0201, 0253 } },
	{ 0xbe87, { 0343, 0201, 0254 } },
	{ 0xbe88, { 0343, 0201, 0255 } },
	{ 0xbe89, { 0343, 0201, 0256 } },
	{ 0xbe8a, { 0343, 0201, 0257 } },
	{ 0xbe8b, { 0343, 0201, 0262 } },
	{ 0xbe8c, { 0343, 0201, 0265 } },
	{ 0xbe8d, { 0343, 0201, 0270 } },
	{ 0xbe8e, { 0343, 0201, 0273 } },
	{ 0xbe8f, { 0343, 0201, 0276 } },
	{ 0xbe90, { 0343, 0201, 0277 } },
	{ 0xbe91, { 0343, 0202, 0200 } },
	{ 0xbe92, { 0343, 0202, 0201 } },
	{ 0xbe93, { 0343, 0202, 0202 } },
	{ 0xbe94, { 0343, 0202, 0204 } },
	{ 0xbe95, { 0343, 0202, 0206 } },
	{ 0xbe96, { 0343, 0202, 0210 } },
	{ 0xbe97, { 0343, 0202, 0211 } },
	{ 0xbe98, { 0343, 0202, 0212 } },
	{ 0xbe99, { 0343, 0202, 0213 } },
	{ 0xbe9a, { 0343, 0202, 0214 } },
	{ 0xbe9b, { 0343, 0202, 0215 } },
	{ 0xbe9c, { 0343, 0202, 0217 } },
	{ 0xbe9d, { 0343, 0202, 0223 } },
};

static const unsigned char DAKUTEN_HALF[3] = { 0xef, 0xbe, 0x9e };
static const unsigned char DAKUTEN_WIDE[3] = { 0xe3, 0x82, 0x9b };
static const unsigned char HANDAKU_HALF[3] = { 0xef, 0xbe, 0x9f };
static const unsigned char HANDAKU_WIDE[3] = { 0xe3, 0x82, 0x9c };

static int
mapcmp(const void *lhs, const void *rhs)
{
	const Map			   *lc = lhs;
	const unsigned short   *rc = rhs;

	if (lc->from < *rc)
		return -1;
	else if (lc->from > *rc)
		return +1;
	else
		return 0;
}

static void
appendMappedChar(StringInfo dst, const unsigned char *src, size_t srclen, const Map *map, size_t maplen, append_t append)
{
	if (srclen == 3)
	{
		unsigned short c = WIDEN(src[1], src[2]);
		const Map *m = bsearch(&c, map, maplen, sizeof(Map), mapcmp);
		if (m != NULL)
		{
			append(dst, m->to, uchar_strlen(m->to));
			return;
		}
	}

	/* 変換対象文字ではなかったので、そのまま連結する. */
	append(dst, src, srclen);
}

#define EQ3(a, b)	(a[0] == b[0] && a[1] == b[1] && a[2] == b[2])

void
normalize_utf8(StringInfo dst, const char *src, size_t srclen, append_t append)
{
	int		len;
	const unsigned char *s = (const unsigned char *)src;
	const unsigned char *end = s + srclen;

	for (; s < end; s += len)
	{
		len = uchar_mblen(s);

		if (len == 1)
		{
			append(dst, s, len);
			continue;
		}

		/* 濁点、半濁点の処理 */
		if (len == 3 && dst->len >= 3 && *StringInfoTail(dst, 3) == 0xe3)
		{
			if (EQ3(s, DAKUTEN_HALF) || EQ3(s, DAKUTEN_WIDE))
			{
				/* 濁点 */
				unsigned char *prev = StringInfoTail(dst, 3);
				switch (WIDEN(prev[1], prev[2]))
				{
				case 0x8186: /* う */
				case 0x82a6: /* ウ */
					prev[1] = 0x83;
					prev[2] = 0xb4;
					continue;
				case 0x82bf: /* タ */
					prev[1] = 0x83;
					prev[2] = 0x80;
					continue;
				case 0x818b: /* か */
				case 0x818d: /* き */
				case 0x818f: /* く */
				case 0x8191: /* け */
				case 0x8193: /* こ */
				case 0x8195: /* さ */
				case 0x8197: /* し */
				case 0x8199: /* す */
				case 0x819b: /* せ */
				case 0x819d: /* そ */
				case 0x819f: /* た */
				case 0x81a1: /* ち */
				case 0x81a4: /* つ */
				case 0x81a6: /* て */
				case 0x81a8: /* と */
				case 0x81af: /* は */
				case 0x81b2: /* ひ */
				case 0x81b5: /* ふ */
				case 0x81b8: /* へ */
				case 0x81bb: /* ほ */
				case 0x82ab: /* カ */
				case 0x82ad: /* キ */
				case 0x82af: /* ク */
				case 0x82b1: /* ケ */
				case 0x82b3: /* コ */
				case 0x82b5: /* サ */
				case 0x82b7: /* シ */
				case 0x82b9: /* ス */
				case 0x82bb: /* セ */
				case 0x82bd: /* ソ */
				case 0x8381: /* チ */
				case 0x8384: /* ツ */
				case 0x8386: /* テ */
				case 0x8388: /* ト */
				case 0x838f: /* ハ */
				case 0x8392: /* ヒ */
				case 0x8395: /* フ */
				case 0x8398: /* ヘ */
				case 0x839b: /* ホ */
					prev[2] += 1;
					continue;
				}
			}
			else if (EQ3(s, HANDAKU_HALF) || EQ3(s, HANDAKU_WIDE))
			{
				/* 半濁点 */
				unsigned char *prev = StringInfoTail(dst, 3);
				switch (WIDEN(prev[1], prev[2]))
				{
				case 0x81af: /* は */
				case 0x81b2: /* ひ */
				case 0x81b5: /* ふ */
				case 0x81b8: /* へ */
				case 0x81bb: /* ほ */
				case 0x838f: /* ハ */
				case 0x8392: /* ヒ */
				case 0x8395: /* フ */
				case 0x8398: /* ヘ */
				case 0x839b: /* ホ */
					prev[2] += 2;
					continue;
				}
			}
		}

		switch (s[0])
		{
		case 0xe2:
			if (s[1] == 0x80)
			{
				switch (s[2])
				{
				case 0x98: /* ` */
					appendStringInfoChar(dst, 0x60);
					break;
				case 0x99: /* ' */
					appendStringInfoChar(dst, 0x27);
					break;
				case 0x9d: /* " */
					appendStringInfoChar(dst, 0x22);
					break;
				default:
					append(dst, s, len);
					break;
				}
			}
			else
				append(dst, s, len);
			break;
		case 0xe3:
			if (s[1] == 0x80 && s[2] == 0x80) /* 全角スペース */
				appendStringInfoChar(dst, 0x20);
			else
				append(dst, s, len);
			break;
		case 0xef: /* 半角カタカナとその他の記号 */
			appendMappedChar(dst, s, len, UTF8_MAP, lengthof(UTF8_MAP), append);
			break;
		default:
			append(dst, s, len);
			break;
		}
	}
}

static const unsigned char DASH[] = { 0xe3, 0x83, 0xbc };
#define CHAR_LEN	3
#define DASH_LEN	3

char *
lexize_utf8(const char *s, size_t len)
{
	const unsigned char *str = (const unsigned char *) s;
	char *r;

	/* 1文字のひらがな, カタカナは削る. */
	if (len == CHAR_LEN && str[0] == 0xe3)
	{
		int	widen = WIDEN(str[1], str[2]);
		if (0x8181 <= widen && widen <= 0x83b6)
			return NULL;
	}

	/* 4文字以上で末尾が「ー」の場合は削る. */
	if (len >= (4 * CHAR_LEN) &&
		memcmp(&str[len - DASH_LEN], DASH, DASH_LEN) == 0)
		len -= DASH_LEN;

	r = palloc(len + 1);
	memcpy(r, str, len);
	r[len] = '\0';

	return r;
}

/*
 * 無視する単語の種類
 */

static const char JOSHI[] = { 0345, 0212, 0251, 0350, 0251, 0236, ',' };
static const char JODOU[] = { 0345, 0212, 0251, 0345, 0213, 0225, 0350, 0251, 0236, ',' };
static const char KIGOU[] = { 0350, 0250, 0230, 0345, 0217, 0267, ',' };
static const char BYWORD[] = { 0345, 0220, 0215, 0350, 0251, 0236, ',', 0344, 0273, 0243, 0345, 0220, 0215, 0350, 0251, 0236, ','};
static const char INSUFF[] = { 0345, 0220, 0215, 0350, 0251, 0236, ',', 0351, 0235, 0236, 0350, 0207, 0252, 0347, 0253, 0213, ','};
static const char KANDO[] = { 0346, 0204, 0237, 0345, 0213, 0225, 0350, 0251, 0236, ','};
static const char FILLER[] = { 0343, 0203, 0225, 0343, 0202, 0243, 0343, 0203, 0251, 0343, 0203, 0274, ','};
static const char OTHERS[] = { 0343, 0201, 0235, 0343, 0201, 0256, 0344, 0273, 0226, ','};

/* for korean */
static const char JOSA_JKS[] = "JKS,";
static const char JOSA_JKC[] = "JKC,";
static const char JOSA_JKG[] = "JKG,";
static const char JOSA_JKO[] = "JKO,";
static const char JOSA_JKB[] = "JKB,";
static const char JOSA_JKV[] = "JKV,";
static const char JOSA_JKQ[] = "JKQ,";
static const char JOSA_JX[] = "JX,";
static const char JOSA_JC[] = "JC,";
static const char KOSIGN_SF[] = "SF,";
static const char KOSIGN_SE[] = "SE,";
static const char KOSIGN_SSO[] = "SSO,";
static const char KOSIGN_SSC[] = "SSC,";
static const char KOSIGN_SC[] = "SC,";
static const char KOSIGN_SY[] = "SY,";
static const char KONN_NNB[] = "NNB,";
static const char KONN_NP[] = "NP,";
static const char KOIC[] = "IC,";
static const char KOTAIL_EP[] = "EP,";
static const char KOTAIL_EF[] = "EF,";
static const char KOTAIL_EC[] = "EC,";
static const char KOTAIL_ETN[] = "ETN,";
static const char KOTAIL_ETM[] = "ETM,";
static const char KOTAIL_XSN[] = "XSN,";
static const char KOTAIL_XSV[] = "XSV,";
static const char KOTAIL_XSA[] = "XSA,";
static const char KOTAIL_VCP[] = "VCP,";
static const char KOTAIL_VCN[] = "VCN,";


const IgnorableWord	IGNORE_UTF8[] =
{
//	{ lengthof(JOSHI), JOSHI },		/* 助詞 */
//	{ lengthof(JODOU), JODOU },		/* 助動詞 */
//	{ lengthof(KIGOU), KIGOU },		/* 記号 */
//	{ lengthof(BYWORD), BYWORD },	/* 名詞,代名詞 */
//	{ lengthof(INSUFF), INSUFF },	/* 名詞,非自立 */
//	{ lengthof(KANDO), KANDO },		/* 感動詞 */
//	{ lengthof(FILLER), FILLER },	/* フィラー */
//	{ lengthof(OTHERS), OTHERS },	/* その他 */
	{ lengthof(JOSA_JKS)-1, JOSA_JKS }, /* 조사들 */
        { lengthof(JOSA_JKC)-1, JOSA_JKC},
        { lengthof(JOSA_JKG)-1, JOSA_JKG},
        { lengthof(JOSA_JKO)-1, JOSA_JKO},
        { lengthof(JOSA_JKB)-1, JOSA_JKB},
        { lengthof(JOSA_JKV)-1, JOSA_JKV},
        { lengthof(JOSA_JKQ)-1, JOSA_JKQ},
        { lengthof(JOSA_JX)-1, JOSA_JX},
        { lengthof(JOSA_JC)-1, JOSA_JC},
        { lengthof(KOSIGN_SF)-1, KOSIGN_SF}, /* 기호들 */
        { lengthof(KOSIGN_SE)-1, KOSIGN_SE},
        { lengthof(KOSIGN_SSO)-1, KOSIGN_SSO},
        { lengthof(KOSIGN_SSC)-1, KOSIGN_SSC},
        { lengthof(KOSIGN_SC)-1, KOSIGN_SC},
        { lengthof(KOSIGN_SY)-1, KOSIGN_SY},
        { lengthof(KONN_NNB)-1, KONN_NNB}, /* 의존명사 */
        { lengthof(KONN_NP)-1, KONN_NP},   /* 대명사 */
        { lengthof(KOIC)-1, KOIC},         /* 감탄사 */
        { lengthof(KOTAIL_EP)-1, KOTAIL_EP},         /* 선어말어미 */
        { lengthof(KOTAIL_EF)-1, KOTAIL_EF},         /* 종결어미 */
        { lengthof(KOTAIL_EC)-1, KOTAIL_EC},         /* 연결어미 */
        { lengthof(KOTAIL_ETN)-1, KOTAIL_ETN},         /* 명사전성어미 */
        { lengthof(KOTAIL_ETM)-1, KOTAIL_ETM},         /* 관형사전성어미 */
        { lengthof(KOTAIL_XSN)-1, KOTAIL_XSN},         /* 접미사 */
        { lengthof(KOTAIL_XSV)-1, KOTAIL_XSV},
        { lengthof(KOTAIL_XSA)-1, KOTAIL_XSA},
        { lengthof(KOTAIL_VCP)-1, KOTAIL_VCP},
        { lengthof(KOTAIL_VCN)-1, KOTAIL_VCN},
	{ 0 }
};

/*
 * カタカナ -> ひらがな
 */
void
hiragana_utf8(StringInfo dst, const char *src, size_t srclen)
{
	int		len;
	const unsigned char *s = (const unsigned char *)src;
	const unsigned char *end = s + srclen;

	for (; s < end; s += len)
	{
		len = uchar_mblen(s);

		if (len != 3)
		{
			appendMBString(dst, s, len);
			continue;
		}

		/* 濁点、半濁点の処理 */
		if (dst->len >= 3 && *StringInfoTail(dst, 3) == 0xe3)
		{
			if (EQ3(s, DAKUTEN_HALF) || EQ3(s, DAKUTEN_WIDE))
			{
				/* 濁点 */
				unsigned char *prev = StringInfoTail(dst, 3);
				switch (WIDEN(prev[1], prev[2]))
				{
				case 0x818b: /* か */
				case 0x818d: /* き */
				case 0x818f: /* く */
				case 0x8191: /* け */
				case 0x8193: /* こ */
				case 0x8195: /* さ */
				case 0x8197: /* し */
				case 0x8199: /* す */
				case 0x819b: /* せ */
				case 0x819d: /* そ */
				case 0x819f: /* た */
				case 0x81a1: /* ち */
				case 0x81a4: /* つ */
				case 0x81a6: /* て */
				case 0x81a8: /* と */
				case 0x81af: /* は */
				case 0x81b2: /* ひ */
				case 0x81b5: /* ふ */
				case 0x81b8: /* へ */
				case 0x81bb: /* ほ */
					prev[2] += 1;
					continue;
				}
			}
			else if (EQ3(s, HANDAKU_HALF) || EQ3(s, HANDAKU_WIDE))
			{
				/* 半濁点 */
				unsigned char *prev = StringInfoTail(dst, 3);
				switch (WIDEN(prev[1], prev[2]))
				{
				case 0x81af: /* は */
				case 0x81b2: /* ひ */
				case 0x81b5: /* ふ */
				case 0x81b8: /* へ */
				case 0x81bb: /* ほ */
					prev[2] += 2;
					continue;
				}
			}
		}

		if (s[0] == 0xe3)
		{
			if (s[1] == 0202 && 0241 <= s[2] && s[2] <= 0277)
			{
				appendStringInfoChar(dst, 0xe3);
				appendStringInfoChar(dst, 0201);
				appendStringInfoChar(dst, s[2] - 040);
				continue;
			}
			else if (s[1] == 0203)
			{
				if (0200 <= s[2] && s[2] <= 0237)
				{
					appendStringInfoChar(dst, 0xe3);
					appendStringInfoChar(dst, 0201);
					appendStringInfoChar(dst, s[2] + 040);
					continue;
				}
				else if (0237 < s[2] && s[2] <= 0263)
				{
					appendStringInfoChar(dst, 0xe3);
					appendStringInfoChar(dst, 0202);
					appendStringInfoChar(dst, s[2] - 040);
					continue;
				}
			}
		}
		else if (s[0] == 0xef)
		{
			/* 半角カタカナ */
			appendMappedChar(dst, s, len, UTF8_HANKANA2ZENHIRA,
				lengthof(UTF8_HANKANA2ZENHIRA), (append_t) appendBinaryStringInfo);
			continue;
		}

		appendMBString(dst, s, len);
	}
}

/*
 * ひらがな -> カタカナ
 */
void
katakana_utf8(StringInfo dst, const char *src, size_t srclen)
{
	int		len;
	const unsigned char *s = (const unsigned char *)src;
	const unsigned char *end = s + srclen;

	for (; s < end; s += len)
	{
		len = uchar_mblen(s);

		if (len == 3 && s[0] == 0xe3)
		{
			if (s[1] == 0201)
			{
				if (0201 <= s[2] && s[2] <= 0237)
				{
					appendStringInfoChar(dst, 0xe3);
					appendStringInfoChar(dst, 0202);
					appendStringInfoChar(dst, s[2] + 040);
					continue;
				}
				else if (0237 < s[2] && s[2] <= 0277)
				{
					appendStringInfoChar(dst, 0xe3);
					appendStringInfoChar(dst, 0203);
					appendStringInfoChar(dst, s[2] - 040);
					continue;
				}
			}
			else if (s[1] == 0202 && 0200 <= s[2] && s[2] <= 0223)
			{
				appendStringInfoChar(dst, 0xe3);
				appendStringInfoChar(dst, 0203);
				appendStringInfoChar(dst, s[2] + 040);
				continue;
			}
		}

		appendMBString(dst, s, len);
	}
}
