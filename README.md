textsearch_ko
=============

PostgreSQL extension module for Korean full text search using mecab

PostgreSQL 데이터베이스 서버에서 사용할 한글 형태소 분석기 기반 전문 검색 모듈 소스입니다. 

# 설치방법

  
## 1. mecab-ko 설치
https://bitbucket.org/eunjeon/mecab-ko

페이지를 참조

aarch64 환경에서는 configure 작업할 때, --build=aarch64-unknown-linux-gnu 옵션 추가해야함. (워낙 오래된 라이브러리라서)
## 2. mecab-ko-dic 설치
https://bitbucket.org/eunjeon/mecab-ko-dic

페이지를 참조
## 3. textsearch_ko 설치
데이터베이스 인코딩은 반드시 utf-8이어야함!
```
export PATH=/opt/mecab-ko/bin:/postgres/15/bin:$PATH
make USE_PGXS=1 install
```
.so 파일의 mecab-ko 라이브러리 rpath 설정하는 방법 모름. 알아서 잘.
## 4. 테스트
```
ioseph@localhost:~/textsearch_ko$ psql
 Pager usage is off.
 psql (9.3.5)
 Type "help" for help.
 
 ioseph=# CREATE EXTENSION textsearch_ko;

 ioseph=# -- 기본 언어를 한국어로 설정
          set default_text_search_config = korean;
 SET
 ioseph=# -- mecab-ko 모듈이 정상 작동하는지 확인
          select * from mecabko_analyze('무궁화꽃이 피었습니다.');
  word  | type | part1st | partlast | pronounce | conjtype | conjugation | basic | detail  |                      lucene
 --------+------+---------+----------+-----------+----------+-------------+-------+---------+---------------------------------------------------
 무궁화 | NNG  |         | F        | 무궁화    | Compound |             |       | 무궁+화 | 무궁/NNG/*/1/1+무궁화/Compound/*/0/2+화/NNG/*/1/1
 꽃     | NNG  |         | T        | 꽃        |          |             |       |         |
 이     | JKS  |         | F        | 이        |          |             |       |         |
 피     | VV   |         | F        | 피        |          |             |       |         |
 었     | EP   |         | T        | 었        |          |             |       |         |
 습니다 | EF   |         | F        | 습니다    |          |             |       |         |
 .      | SF   |         |          | .         |          |             |       |         |
 (7 rows)
 
 ioseph=#  -- 필요없는 조사, 어미들을 빼고 백터로 만드는지 확인 
         select * from to_tsvector('무궁화꽃이 피었습니다.');
       to_tsvector
 --------------------------
 '꽃':2 '무궁화':1 '피':3
 (1 row)
 
  ioseph=# select * from to_tsvector('그래서, 무궁화꽃이 피겠는걸요?');
       to_tsvector
 --------------------------
  '꽃':2 '무궁화':1 '피':3
 (1 row)
```
