SET search_path = public;

BEGIN;

--
-- Korean text parser
--

CREATE FUNCTION ts_mecabko_start(internal, int4)
    RETURNS internal
    AS '$libdir/ts_mecab_ko'
    LANGUAGE 'c' STRICT;

CREATE FUNCTION ts_mecabko_gettoken(internal, internal, internal)
    RETURNS internal
    AS '$libdir/ts_mecab_ko'
    LANGUAGE 'c' STRICT;

CREATE FUNCTION ts_mecabko_end(internal)
    RETURNS void
    AS '$libdir/ts_mecab_ko'
    LANGUAGE 'c' STRICT;

CREATE TEXT SEARCH PARSER pg_catalog.korean (
    START    = ts_mecabko_start,
    GETTOKEN = ts_mecabko_gettoken,
    END      = ts_mecabko_end,
    HEADLINE = pg_catalog.prsd_headline,
    LEXTYPES = pg_catalog.prsd_lextype
);
COMMENT ON TEXT SEARCH PARSER pg_catalog.korean IS
    'korean word parser';

--
-- Korean text lexizer
--

CREATE FUNCTION ts_mecabko_lexize(internal, internal, internal, internal)
    RETURNS internal
    AS '$libdir/ts_mecab_ko'
    LANGUAGE 'c' STRICT;

CREATE TEXT SEARCH TEMPLATE pg_catalog.mecabko (
	LEXIZE = ts_mecabko_lexize
);

CREATE TEXT SEARCH DICTIONARY pg_catalog.korean_stem (
	TEMPLATE = pg_catalog.mecabko
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

CREATE FUNCTION mecabko_analyze(
        text,
        OUT word text,
        OUT type text,
        OUT part1st text,
        OUT partlast text,
        OUT pronounce text,
        OUT conjtype text,
        OUT conjugation text,
        OUT basic text,
        OUT detail text,
        OUT lucene text)
    RETURNS SETOF record
    AS '$libdir/ts_mecab_ko'
    LANGUAGE 'c' IMMUTABLE STRICT;

CREATE FUNCTION korean_normalize(text)
    RETURNS text
    AS '$libdir/ts_mecab_ko'
    LANGUAGE 'c' IMMUTABLE STRICT;

CREATE FUNCTION hanja2hangul(text)
    RETURNS text
    AS '$libdir/ts_mecab_ko'
    LANGUAGE 'c' IMMUTABLE STRICT;

COMMIT;
