SET search_path = public;

BEGIN;

--
-- Korean text parser
--

CREATE FUNCTION ts_ja_start(internal, int4)
    RETURNS internal
    AS '$libdir/textsearch_ja'
    LANGUAGE 'c' STRICT;

CREATE FUNCTION ts_ja_gettoken(internal, internal, internal)
    RETURNS internal
    AS '$libdir/textsearch_ja'
    LANGUAGE 'c' STRICT;

CREATE FUNCTION ts_ja_end(internal)
    RETURNS void
    AS '$libdir/textsearch_ja'
    LANGUAGE 'c' STRICT;

CREATE TEXT SEARCH PARSER pg_catalog.korean (
    START    = ts_ja_start,
    GETTOKEN = ts_ja_gettoken,
    END      = ts_ja_end,
    HEADLINE = pg_catalog.prsd_headline,
    LEXTYPES = pg_catalog.prsd_lextype
);
COMMENT ON TEXT SEARCH PARSER pg_catalog.korean IS
    'korean word parser';

--
-- Korean text lexizer
--

CREATE FUNCTION ts_ja_lexize(internal, internal, internal, internal)
    RETURNS internal
    AS '$libdir/textsearch_ja'
    LANGUAGE 'c' STRICT;

CREATE TEXT SEARCH TEMPLATE pg_catalog.mecab (
	LEXIZE = ts_ja_lexize
);

CREATE TEXT SEARCH DICTIONARY pg_catalog.korean_stem (
	TEMPLATE = pg_catalog.mecab
);

--
-- Korean text configuration
--

CREATE TEXT SEARCH CONFIGURATION pg_catalog.korean (PARSER = korean);
COMMENT ON TEXT SEARCH CONFIGURATION pg_catalog.korean IS
    'configuration for korean language';

ALTER TEXT SEARCH CONFIGURATION pg_catalog.korean ADD MAPPING
    FOR email, url, url_path, host, file, version,
        sfloat, float, int, uint,
        numword, hword_numpart, numhword
    WITH simple;

-- Default configuration is Korean-English.
-- Replace english_stem if you use other language.
ALTER TEXT SEARCH CONFIGURATION pg_catalog.korean ADD MAPPING
    FOR asciiword, hword_asciipart, asciihword
    WITH english_stem;

ALTER TEXT SEARCH CONFIGURATION pg_catalog.korean ADD MAPPING
    FOR word, hword_part, hword
    WITH korean_stem;

--
-- Utility functions
--

CREATE FUNCTION ja_analyze(
        text,
        OUT word text,
        OUT type text,
        OUT subtype1 text,
        OUT subtype2 text,
        OUT subtype3 text,
        OUT conjtype text,
        OUT conjugation text,
        OUT basic text,
        OUT ruby text,
        OUT pronounce text)
    RETURNS SETOF record
    AS '$libdir/textsearch_ja'
    LANGUAGE 'c' IMMUTABLE STRICT;

CREATE FUNCTION ja_normalize(text)
    RETURNS text
    AS '$libdir/textsearch_ja'
    LANGUAGE 'c' IMMUTABLE STRICT;

CREATE FUNCTION ja_wakachi(text)
    RETURNS text
    AS '$libdir/textsearch_ja'
    LANGUAGE 'c' IMMUTABLE STRICT;

CREATE FUNCTION web_query(text) RETURNS text AS
$$
  SELECT regexp_replace(regexp_replace(regexp_replace($1,
    E'(^|\\s+)-', E'\\1!', 'g'),
    E'\\s+OR\\s+', '|', 'g'),
    E'\\s+', '&', 'g');
$$
LANGUAGE sql IMMUTABLE STRICT;

CREATE FUNCTION furigana(text)
    RETURNS text
    AS '$libdir/textsearch_ja'
    LANGUAGE 'c' IMMUTABLE STRICT;

CREATE FUNCTION hiragana(text)
    RETURNS text
    AS '$libdir/textsearch_ja'
    LANGUAGE 'c' IMMUTABLE STRICT;

CREATE FUNCTION katakana(text)
    RETURNS text
    AS '$libdir/textsearch_ja'
    LANGUAGE 'c' IMMUTABLE STRICT;

--

CREATE FUNCTION hanja2hangul(text)
    RETURNS text
    AS '$libdir/textsearch_ja'
    LANGUAGE 'c' IMMUTABLE STRICT;


COMMIT;
