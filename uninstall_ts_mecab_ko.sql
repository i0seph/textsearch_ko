SET search_path = public;

DROP TEXT SEARCH CONFIGURATION korean CASCADE;
DROP TEXT SEARCH PARSER korean CASCADE;
DROP TEXT SEARCH DICTIONARY korean_stem CASCADE;
DROP TEXT SEARCH TEMPLATE mecabko CASCADE;

DROP FUNCTION ts_mecabko_start(internal, int4);
DROP FUNCTION ts_mecabko_gettoken(internal, internal, internal);
DROP FUNCTION ts_mecabko_end(internal);
DROP FUNCTION ts_mecabko_lexize(internal, internal, internal, internal);
DROP FUNCTION mecabko_analyze(text);
DROP FUNCTION korean_normalize(text);
DROP FUNCTION hanja2hangul(text);
