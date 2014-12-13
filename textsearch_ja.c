/*
 * textsearch_ja.c
 */
#include "postgres.h"

#include "access/heapam.h"
#include "access/htup_details.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "mb/pg_wchar.h"
#include "tsearch/ts_public.h"
#include "tsearch/ts_utils.h"
#include "utils/builtins.h"

#include "textsearch_ja.h"
#include <mecab.h>
#include "pgut/pgut-be.h"

PG_MODULE_MAGIC;

/*
 * 日本語単語を含む可能性のあるデフォルトパーサの単語
 */
#define WORD_T			2	/* Word, all letters */
#define NUMWORD			3	/* Word, letters and digits */
#define NUMPARTHWORD	9	/* Hyphenated word part, letters and digits */
#define PARTHWORD		10	/* Hyphenated word part, all letters */
#define NUMHWORD		15	/* Hyphenated word, letters and digits */
#define HWORD			17	/* Hyphenated word, all letters */

#define IS_JWORD(t)	( \
	(t) == WORD_T || (t) == NUMWORD || (t) == NUMPARTHWORD || \
	(t) == PARTHWORD || (t) == NUMHWORD || (t) == HWORD)

/* 空白 */
#define SPACE			12	/* Space symbols */

/* MeCab 辞書の内容 (IPA辞書に依存している可能性がある) */
#define NUM_CSV			9
#define MECAB_BASIC		3	/* 基本形 */
#define MECAB_RUBY		7	/* ルビ */
#define MECAB_SORI		3	/* 한자 */
#define MECAB_CONJTYPE		4	/* 용언활용 */

#define SEPARATOR_CHAR	'\v'

/*
 * ts_ja_parser - 解析中のテキストを保存する.
 */
typedef struct ts_ja_parser
{
	StringInfoData		str;
	const mecab_node_t *node;		/* japanese parser */
	Datum				ascprs;		/* ascii word parser */
	const char		   *ja_pos;
} ts_ja_parser;

PG_FUNCTION_INFO_V1(ts_ja_start);
PG_FUNCTION_INFO_V1(ts_ja_gettoken);
PG_FUNCTION_INFO_V1(ts_ja_end);
PG_FUNCTION_INFO_V1(ts_ja_lexize);
PG_FUNCTION_INFO_V1(ja_analyze);
PG_FUNCTION_INFO_V1(ja_normalize);
PG_FUNCTION_INFO_V1(ja_wakachi);
PG_FUNCTION_INFO_V1(furigana);
PG_FUNCTION_INFO_V1(hiragana);
PG_FUNCTION_INFO_V1(katakana);
PG_FUNCTION_INFO_V1(hanja2hangul);

extern void PGDLLEXPORT _PG_init(void);
extern void PGDLLEXPORT _PG_fini(void);
extern Datum PGDLLEXPORT ts_ja_start(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT ts_ja_gettoken(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT ts_ja_end(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT ts_ja_lexize(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT ja_analyze(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT ja_normalize(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT ja_wakachi(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT furigana(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT hiragana(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT katakana(PG_FUNCTION_ARGS);
extern Datum PGDLLEXPORT hanja2hangul(PG_FUNCTION_ARGS);

static bool	feature(const mecab_node_t *node, int n, const char **t, int *tlen);
static void	normalize(StringInfo dst, const char *src, size_t srclen, append_t append);
static char *lexize(const char *str, size_t len);
static bool	ignore(const mecab_node_t *node);
static bool	ignore_mecab_ko_part(const char *str, int slen);
static void appendString(StringInfo dst, const unsigned char *src, int srclen);

/* mecab-ko-dic 에서 사용할 품사들 */

static char *accept_parts_of_speech[13] = {
	"NNG" ,"NNP" ,"NNB" ,"NNBC" ,"NR" ,"VV" ,"VA" ,"MM" ,"MAG" ,"XSN" ,"XR" ,"SH", ""
};

/* mecab */
static mecab_t	   *_mecab;

/*
 * mecab_assert - 失敗したら終了.
 */
#define mecab_assert(expr) \
	if (expr); else \
		ereport(ERROR, \
			(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION), \
			 errmsg("mecab: %s", mecab_strerror(_mecab))))

/*
 * mecab_assert_encoding - 辞書とDBのエンコーディングが異なると終了.
 */

static int	mecab_dict_encoding = -1;

static mecab_t *
mecab_acquire(void)
{
	if (mecab_dict_encoding < 0)
	{
		const mecab_dictionary_info_t *dict = mecab_dictionary_info(_mecab);
		int		encoding = pg_char_to_encoding(dict->charset);

		if (encoding != GetDatabaseEncoding())
		{
			ereport(ERROR,
				(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
				errmsg("mecab: encoding mismatch (db=%s, mecab=%s)",
					GetDatabaseEncodingName(), dict->charset)));
		}

		mecab_dict_encoding = encoding;
	}

	return _mecab;
}

/*
 * _PG_init - DLL ロード時に呼ばれる. mecab を初期化する.
 */
void
_PG_init(void)
{
	if (_mecab == NULL)
	{
		int			argc = 3;
		char	   *argv[] = { "mecab", "-O", "wakati" };
		_mecab = mecab_new(argc, argv);
		mecab_assert(_mecab);
	}
}

/*
 * _PG_init - DLL アンロード時に呼ばれる. mecab を破棄する.
 */
void
_PG_fini(void)
{
	if (_mecab != NULL)
	{
		mecab_destroy(_mecab);
		_mecab = NULL;
	}
}

/*
 * ts_ja_start - パーサを初期化する.
 */
Datum
ts_ja_start(PG_FUNCTION_ARGS)
{
	mecab_t			   *mecab = mecab_acquire();
	char			   *input = (char *) PG_GETARG_POINTER(0);
	int					len	= PG_GETARG_INT32(1);
	ts_ja_parser	   *parser;

	parser = (ts_ja_parser *) palloc(sizeof(ts_ja_parser));
	initStringInfo(&parser->str);
	/*
	 * XXX: 한국형 일반화
	 */
	normalize(&parser->str, input, len, appendString);

	/* Replace input text to the normalized text. */
	input = parser->str.data;
	len = parser->str.len;

	/*
	 * XXX: ts_ja_xxx の呼び出しがオーバーラップしてもよいよう
	 * node をコピーすべきかもしれない.
	 */
	parser->node = mecab_sparse_tonode2(mecab, input, len);
	mecab_assert(parser->node);

	/* 英数字文字列の解析のため、デフォルトパーサも初期化する. */
	parser->ascprs = DirectFunctionCall2(
		prsd_start, CStringGetDatum(input), Int32GetDatum(len));
	parser->ja_pos = NULL;

    PG_RETURN_POINTER(parser);
}

static const mecab_node_t *
ja_gettoken(ts_ja_parser *parser)
{
	const mecab_node_t *result;

	for (; parser->node != NULL; parser->node = parser->node->next)
	{
		/* 文頭, 文末は無視. */
		switch (parser->node->stat)
		{
		case MECAB_BOS_NODE:
		case MECAB_EOS_NODE:
			continue;
		}

		result = parser->node;
		parser->node = parser->node->next;
		return result;
	}

	return NULL;	/* 末尾 */
}

/*
 * FIXME: グローバル変数 current_node 経由で処理中の node を渡すのは非常に危険
 * なのだが、他に ts_headline に対応する方法が無いので仕方なくこの方法を取っている.
 * この方式だと、ts_debug() が期待通りに動作しない問題がある.
 */
static const mecab_node_t *current_node;

Datum
ts_ja_gettoken(PG_FUNCTION_ARGS)
{
	ts_ja_parser   *parser = (ts_ja_parser *) PG_GETARG_POINTER(0);
	const char	  **t = (const char **) PG_GETARG_POINTER(1);
	int			   *tlen  = (int *) PG_GETARG_POINTER(2);
	int				lextype;
	const char	   *skip;
	const mecab_node_t *node;

	current_node = NULL;

	if (parser->ja_pos == NULL)
	{
		for (;;)
		{
			lextype = DatumGetInt32(DirectFunctionCall3(
				prsd_nexttoken, parser->ascprs,
				PointerGetDatum(t), PointerGetDatum(tlen)));

			if (lextype == 0)
			{
				/* 解析完了 */
				PG_RETURN_INT32(0);
			}
			else if (lextype == SPACE && *tlen > 0 && **t == SEPARATOR_CHAR)
			{
				/* ダミーセパレータは無視する. */
				continue;
			}
			else if (IS_JWORD(lextype))
			{
				/* 日本語単語の場合は、MeCabでの解析を行う. */
				skip = *t;
				parser->ja_pos = *t + *tlen;
				break;
			}
			else
			{
				/* ASCII単語の場合は、ASCIIパーサの結果をそのまま返す. */
				parser->ja_pos = NULL;
				PG_RETURN_INT32(lextype);
			}
		}
	}
	else
		skip = NULL;

	do
	{
		node = ja_gettoken(parser);
		if (node == NULL)
			PG_RETURN_INT32(0);
	} while (node->surface < skip);

	/* 無視する品詞は blank 扱い. */
	if (ignore(node))
		lextype = SPACE;
	else
		lextype = WORD_T;

	*t = node->surface;
	*tlen = node->length;
	if (*t + *tlen >= parser->ja_pos)
		parser->ja_pos = NULL;

	current_node = node;

	PG_RETURN_INT32(lextype);
}

/*
 * ts_ja_end - メモリの解放.
 */
Datum
ts_ja_end(PG_FUNCTION_ARGS)
{
	ts_ja_parser *parser = (ts_ja_parser *) PG_GETARG_POINTER(0);

	current_node = NULL;

	DirectFunctionCall1(prsd_end, parser->ascprs);

	pfree(parser->str.data);
	pfree(parser);

	PG_RETURN_VOID();
}

Datum
ts_ja_lexize(PG_FUNCTION_ARGS)
{

	const char *t = (char *) PG_GETARG_POINTER(1);
	int			tlen = PG_GETARG_INT32(2);
	TSLexeme   *res;
	int pluscnt = 0;
	const char *pluspos;
	const char *commapos;
	const char *slashpos;
	int i;

	if (current_node) {
		if ((feature(current_node, MECAB_CONJTYPE, &t, &tlen))
		   && (strncmp(t, "Inflect,", 8) == 0)
		   && (feature(current_node, MECAB_RUBY, &t, &tlen))){
			do {
				pluspos = strchr(t, '+');
				pluscnt += 1;
				if(pluspos != NULL) t = pluspos + 1;
			} while (pluspos != NULL);
			feature(current_node, MECAB_RUBY, &t, &tlen);
			res = palloc0(sizeof(TSLexeme) * (pluscnt + 1));
			i = 0;
			do {
				pluspos = strchr(t, '+');
				slashpos = strchr(t, '/');
				/* ignore_mecab_ko_part 호출해서 제외 품사면 통과 */
				if(pluspos != NULL) {
					if(ignore_mecab_ko_part(slashpos + 1, pluspos - slashpos - 1)){
						res[i].lexeme = lexize(t, slashpos - t);
						i += 1;
					}
					t = pluspos + 1;
				}
				else {
					commapos = strchr(t, ',');
					if(ignore_mecab_ko_part(slashpos + 1, commapos - slashpos - 1)){
						res[i].lexeme = lexize(t, slashpos - t);
						i += 1;
					}
				}
			} while (pluspos != NULL);
		}
		else {
			res = palloc0(sizeof(TSLexeme) * 2);
			feature(current_node, MECAB_BASIC, &t, &tlen);
			res[0].lexeme = lexize(t, tlen);
		}
	}
	else {
		res = palloc0(sizeof(TSLexeme) * 2);
		res[0].lexeme = lexize(t,tlen);
	}

	PG_RETURN_POINTER(res);
}

#define make_text(s, ln) \
	PointerGetDatum(cstring_to_text_with_len((s), (ln)))

/*
 * ja_analyze - 文字を形態素解析する.
 */
Datum
ja_analyze(PG_FUNCTION_ARGS)
{
	mecab_t			   *mecab = mecab_acquire();
	FuncCallContext	   *funcctx;
	List			   *tuples;
	HeapTuple			tuple;
	HeapTuple			result;

	if (SRF_IS_FIRSTCALL())
	{
		text			   *txt = PG_GETARG_TEXT_PP(0);
		const mecab_node_t *node;
		TupleDesc			tupdesc;

		if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
			elog(ERROR, "return and sql tuple descriptions are incompatible");

		node = mecab_sparse_tonode2(mecab,
				VARDATA_ANY(txt), VARSIZE_ANY_EXHDR(txt));
		mecab_assert(node);

		funcctx = SRF_FIRSTCALL_INIT();

		tuples = NIL;
		for (; node != NULL; node = node->next)
		{
			int				i;
			Datum			values[NUM_CSV+1];
			bool			nulls[NUM_CSV+1] = { 0 };
			const char	   *csv;
			int isskip = 0;
			const char         *conjtype;
			int                conjlen;
			const char *commapos;
			const char *pluspos;
			const char *slashpos;

			

			MemoryContext	ctx;

			/* 시작과 끝 무시 */
			switch (node->stat)
			{
			case MECAB_BOS_NODE:
			case MECAB_EOS_NODE:
				continue;
			}

			/* 단어 처리
                         * conjtype 값이 Inflect 이면, 
                         * ruby 기준으로 row로 분리 */

			csv = node->feature;
			if (feature(node, MECAB_CONJTYPE, &conjtype, &conjlen)){
				if (strncmp(conjtype, "Inflect,", 8) == 0){
					/* 용언 상세 정보로 처리, 없으면 그대로 */
					if (feature(node, MECAB_RUBY, &conjtype, &conjlen)){
						commapos = strchr(conjtype, ',');
						do {
							pluspos = strchr(conjtype, '+');
							slashpos = strchr(conjtype , '/');
							values[0] = make_text(conjtype, slashpos - conjtype);
							for (i = 1; i <= NUM_CSV; i++)
							{
								if(i == 1){
									if(pluspos == NULL){
										values[i] = make_text(slashpos + 1, commapos - slashpos - 1);
									}
									else {
										values[i] = make_text(slashpos + 1, pluspos - slashpos - 1);
									}
								}
								else if(i==3){
									values[i] = make_text("F",1);
								}
								else if(i==4){
									values[i] = make_text(conjtype, slashpos - conjtype);
								}
								else {
									nulls[i] = true;
								}
							}
							ctx = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
							tuple = heap_form_tuple(tupdesc, values, nulls);
							tuples = lappend(tuples, tuple);
							MemoryContextSwitchTo(ctx);
							conjtype = pluspos + 1;
						} while(pluspos != NULL);
						isskip = 1;
					}
				}
				continue;
			}
			if (isskip == 0){
				values[0] = make_text(node->surface, node->length);


				for (i = 1; i <= NUM_CSV; i++)
				{
					const char *n = strchr(csv, ',');
					size_t		len = (n == NULL ? strlen(csv) : n - csv);

					if (len == 0 || (len == 1 && csv[0] == '*'))
					{
						if (i == MECAB_BASIC + 1)
							values[i] = make_text(node->surface, node->length);
						else
							nulls[i] = true;
					}
					else{
						values[i] = make_text(csv, len);
					}

					if (n == NULL)
					{
						for (++i; i <= MECAB_BASIC; i++)
							nulls[i] = true;
						/* 未知語 */
						for (; i <= NUM_CSV; i++)
							values[i] = make_text(node->surface, node->length);
						break;
					}

					csv = n + 1;
				}
				ctx = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
				tuple = heap_form_tuple(tupdesc, values, nulls);
				tuples = lappend(tuples, tuple);
				MemoryContextSwitchTo(ctx);
			}

		}

		funcctx->max_calls = list_length(tuples);
		funcctx->user_fctx = tuples;

		PG_FREE_IF_COPY(txt, 0);
	}
	else
	{
		funcctx = SRF_PERCALL_SETUP();
		tuples = funcctx->user_fctx;
	}

	if (tuples == NIL)
		SRF_RETURN_DONE(funcctx);

	tuple = linitial(tuples);
	funcctx->user_fctx = tuples = list_delete_first(tuples);
	result = heap_copytuple(tuple);
	pfree(tuple);

	SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(result));
}

/*
 * ja_normalize - 文字を正規化する.
 */
Datum
ja_normalize(PG_FUNCTION_ARGS)
{
	Datum			r;
	text		   *txt = PG_GETARG_TEXT_PP(0);
	StringInfoData	str;

	initStringInfo(&str);
	normalize(&str, VARDATA_ANY(txt), VARSIZE_ANY_EXHDR(txt), (append_t) appendBinaryStringInfo);
	PG_FREE_IF_COPY(txt, 0);

	r = CStringGetTextDatum(str.data);
	pfree(str.data);

	PG_RETURN_DATUM(r);
}

/*
 * ja_wakachi - スペース区切りで分かち書きする.
 */
Datum
ja_wakachi(PG_FUNCTION_ARGS)
{
	mecab_t		   *mecab = mecab_acquire();
	text		   *r;
	text		   *txt = PG_GETARG_TEXT_PP(0);
	const char	   *wakati;
	size_t			rlen;

	wakati = mecab_sparse_tostr2(mecab, VARDATA_ANY(txt), VARSIZE_ANY_EXHDR(txt));
	mecab_assert(wakati);
	PG_FREE_IF_COPY(txt, 0);

	/*
	 * 末尾に改行が付けられるので, 取り除く.
	 * FIXME: 元テキストが改行で終わっている場合には取り除くべきではない.
	 */
	rlen = strlen(wakati);
	while (rlen > 0 && (wakati[rlen-1] == ' ' || wakati[rlen-1] == '\n'))
		rlen--;

	r = (text *) palloc(rlen + VARHDRSZ);
	SET_VARSIZE(r, rlen + VARHDRSZ);

	memcpy(VARDATA(r), wakati, rlen);

	PG_RETURN_TEXT_P(r);
}

/*
 * furigana - フリガナを得る.
 */
Datum
furigana(PG_FUNCTION_ARGS)
{
	mecab_t		   *mecab = mecab_acquire();
	text		   *txt = PG_GETARG_TEXT_PP(0);
	StringInfoData	str;
	const mecab_node_t *node;

	node = mecab_sparse_tonode2(mecab,
			VARDATA_ANY(txt), VARSIZE_ANY_EXHDR(txt));
	mecab_assert(node);

	initStringInfo(&str);

	for (; node != NULL; node = node->next)
	{
		const char	   *ruby;
		int				rubylen;

		/* 文頭, 文末は無視. */
		switch (node->stat)
		{
		case MECAB_BOS_NODE:
		case MECAB_EOS_NODE:
			continue;
		}

		if (feature(node, MECAB_RUBY, &ruby, &rubylen))
			appendBinaryStringInfo(&str, ruby, rubylen);
		else
			appendBinaryStringInfo(&str, node->surface, node->length);
	}

	PG_FREE_IF_COPY(txt, 0);

	PG_RETURN_DATUM(CStringGetTextDatum(str.data));
}

/*
 * カタカナ -> ひらがな
 */
Datum
hiragana(PG_FUNCTION_ARGS)
{
	text		   *txt = PG_GETARG_TEXT_PP(0);
	const char	   *src = VARDATA_ANY(txt);
	size_t			srclen = VARSIZE_ANY_EXHDR(txt);
	StringInfoData	str;

	initStringInfo(&str);

	switch (GetDatabaseEncoding())
	{
	case PG_UTF8:
		hiragana_utf8(&str, src, srclen);
		break;
	case PG_EUC_JP:
	case PG_EUC_JIS_2004:
		hiragana_eucjp(&str, src, srclen);
		break;
	default: /* not supported */
		appendBinaryStringInfo(&str, src, srclen);
		break;
	}

	PG_FREE_IF_COPY(txt, 0);

	PG_RETURN_DATUM(CStringGetTextDatum(str.data));
}

/*
 * ひらがな -> カタカナ
 */
Datum
katakana(PG_FUNCTION_ARGS)
{
	text		   *txt = PG_GETARG_TEXT_PP(0);
	const char	   *src = VARDATA_ANY(txt);
	size_t			srclen = VARSIZE_ANY_EXHDR(txt);
	StringInfoData	str;

	initStringInfo(&str);

	switch (GetDatabaseEncoding())
	{
	case PG_UTF8:
		katakana_utf8(&str, src, srclen);
		break;
	case PG_EUC_JP:
	case PG_EUC_JIS_2004:
		katakana_eucjp(&str, src, srclen);
		break;
	default: /* not supported */
		appendBinaryStringInfo(&str, src, srclen);
		break;
	}

	PG_FREE_IF_COPY(txt, 0);

	PG_RETURN_DATUM(CStringGetTextDatum(str.data));
}

/*
 * hanja2hangul - 한자를 한글로 변환
 */
Datum
hanja2hangul(PG_FUNCTION_ARGS)
{
	mecab_t		   *mecab = mecab_acquire();
	text		   *txt = PG_GETARG_TEXT_PP(0);
	StringInfoData	str;
	const mecab_node_t *node;

	node = mecab_sparse_tonode2(mecab,
			VARDATA_ANY(txt), VARSIZE_ANY_EXHDR(txt));
	mecab_assert(node);

	initStringInfo(&str);

	for (; node != NULL; node = node->next)
	{
		const char	   *ruby;
		int				rubylen;

		/* 文頭, 文末は無視. */
		switch (node->stat)
		{
		case MECAB_BOS_NODE:
		case MECAB_EOS_NODE:
			continue;
		}

		if (feature(node, MECAB_SORI, &ruby, &rubylen))
			appendBinaryStringInfo(&str, ruby, rubylen);
		else
			appendBinaryStringInfo(&str, node->surface, node->length);
	}

	PG_FREE_IF_COPY(txt, 0);

	PG_RETURN_DATUM(CStringGetTextDatum(str.data));
}


/*
 * feature - CSV文字列のn番目の列を返す.
 */
static bool
feature(const mecab_node_t *node, int n, const char **t, int *tlen)
{
	const char *csv = node->feature;
	int			i;
	const char *next;
	size_t		len;

	for (i = 0; i < n; i++)
	{
		next = strchr(csv, ',');
		if (next == NULL)
			return false;
		csv = next + 1;
	}

	next = strchr(csv, ',');
	len = (next == NULL ? strlen(csv) : next - csv);

	if (len == 0 || (len == 1 && csv[0] == '*'))
		return false;

	*t = csv;
	*tlen = len;
	return true;
}

/*
 * normalize - 文字を正規化する.
 *  カタカナ : 半角 -> 全角
 *  英数文字 : 全角 -> 半角
 */
static void
normalize(StringInfo dst, const char *src, size_t srclen, append_t append)
{
	switch (GetDatabaseEncoding())
	{
	case PG_UTF8:
		normalize_utf8(dst, src, srclen, append);
		break;
	case PG_EUC_JP:
	case PG_EUC_JIS_2004:
		normalize_eucjp(dst, src, srclen, append);
		break;
	default: /* not supported */
		appendBinaryStringInfo(dst, src, srclen);
		break;
	}
}

/*
 * lexize - 単語を正規化する.
 */
static char *
lexize(const char *str, size_t len)
{
	char *r;

		r = palloc(len + 1);
		memcpy(r, str, len);
		r[len] = '\0';

	return r;

	switch (GetDatabaseEncoding())
	{
	case PG_UTF8:
		r = lexize_utf8(str, len);
		break;
	case PG_EUC_JP:
	case PG_EUC_JIS_2004:
		r = lexize_eucjp(str, len);
		break;
	default: /* not supported */
		r = palloc(len + 1);
		memcpy(r, str, len);
		r[len] = '\0';
		break;
	}

	return r;
}

/*
 * ignore - 無視すべき単語か?
 */
static bool
ignore(const mecab_node_t *node)
{
	const IgnorableWord *ignore;

	switch (GetDatabaseEncoding())
	{
	case PG_UTF8:
		ignore = IGNORE_UTF8;
		break;
	case PG_EUC_JP:
	case PG_EUC_JIS_2004:
		ignore = IGNORE_EUCJP;
		break;
	default: /* not supported */
		return false;
	}

	/* feature は CSV であり、最初の列が品詞を表す. */
	for (; ignore->len > 0; ignore++)
	{
		if (strncmp(node->feature, ignore->word, ignore->len) == 0)
			return true;
	}

	return false;
}

static bool
ignore_mecab_ko_part(const char* str, int slen){
	bool isfind = false;
	int i=0;
	char input_str[5];
	strncpy(input_str, str, slen);
	input_str[slen] = '\0';
	while(1){
		if(strcmp(accept_parts_of_speech[i], "") == 0) break;
		if(strncmp(accept_parts_of_speech[i], input_str, slen +1) == 0){
			isfind = true;
			break;
		}
		i += 1;
	}
	return isfind;
}

/*
 * 全角文字列と半角文字連結する場合は間にスペースを補う.
 */
static void
appendString(StringInfo dst, const unsigned char *src, int srclen)
{
	if (srclen == 1 && !isprint(src[0]))
	{
		if (dst->len == 0 || *StringInfoTail(dst, 1) != SEPARATOR_CHAR)
			appendStringInfoChar(dst, SEPARATOR_CHAR);
	}
	else if (dst->len > 1)
	{
		if (*StringInfoTail(dst, 1) == SEPARATOR_CHAR)
		{
			bool ishigh = IS_HIGHBIT_SET(*StringInfoTail(dst, 2));
			if (srclen == 1 && !ishigh)
			{
				/* "A[改行]A" ⇒ "A A" */
				*StringInfoTail(dst, 1) = ' ';
			}
			else if (ishigh)
			{
				/* "あ[改行]あ" ⇒ "ああ" */
				dst->len--;
			}
		}
		else
		{
			bool ishigh = IS_HIGHBIT_SET(*StringInfoTail(dst, 1));
			if ((srclen == 1 && ishigh) || (srclen > 1 && !ishigh))
				appendStringInfoChar(dst, SEPARATOR_CHAR);
		}
		appendBinaryStringInfo(dst, (const char *) src, srclen);
	}
	else
	{
		appendBinaryStringInfo(dst, (const char *) src, srclen);
	}
}
