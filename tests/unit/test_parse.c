/*
 * Unit tests: COM_Parse / COM_ParseExt / COM_ParseComplex / COM_Compress /
 * SkipBracedSection / SkipRestOfLine (q_shared.c).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qcommon/q_shared.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

#define ASSERT_STREQ(a, b, msg) do { \
	if (strcmp((a), (b)) != 0) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

#define ASSERT_EQ(a, b, msg) do { \
	if ((int)(a) != (int)(b)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

static int test_parse_simple_tokens(void)
{
	const char *s = "  alpha beta  ";
	const char *p = s;

	COM_BeginParseSession( "test" );
	ASSERT_STREQ( COM_Parse( &p ), "alpha", "first token" );
	ASSERT_STREQ( COM_Parse( &p ), "beta", "second token" );
	ASSERT_STREQ( COM_Parse( &p ), "", "EOF empty" );
	ASSERT( p == NULL, "data_p NULL at EOF" );
	return 0;
}

static int test_parse_quoted_spaces(void)
{
	const char *s = "  \"hello world\"  tail ";
	const char *p = s;

	COM_BeginParseSession( "test" );
	ASSERT_STREQ( COM_Parse( &p ), "hello world", "quoted with spaces" );
	ASSERT_STREQ( COM_Parse( &p ), "tail", "after quoted" );
	return 0;
}

static int test_parse_line_comment(void)
{
	const char *s = "a // comment\nb";
	const char *p = s;

	COM_BeginParseSession( "test" );
	ASSERT_STREQ( COM_Parse( &p ), "a", "before //" );
	ASSERT_STREQ( COM_Parse( &p ), "b", "after line comment" );
	return 0;
}

static int test_parse_block_comment(void)
{
	const char *s = "x /* inner */ y";
	const char *p = s;

	COM_BeginParseSession( "test" );
	ASSERT_STREQ( COM_Parse( &p ), "x", "before block comment" );
	ASSERT_STREQ( COM_Parse( &p ), "y", "after block comment" );
	return 0;
}

static int test_parse_quote_then_comment(void)
{
	const char *s = "\"a\" /*c*/ b";
	const char *p = s;

	COM_BeginParseSession( "test" );
	ASSERT_STREQ( COM_Parse( &p ), "a", "quoted then block" );
	ASSERT_STREQ( COM_Parse( &p ), "b", "token after quoted+block" );
	return 0;
}

static int test_parse_line_comment_after_token(void)
{
	/* Need whitespace before // or it is part of the same word token */
	const char *s = "tok //not\nnext";
	const char *p = s;

	COM_BeginParseSession( "test" );
	ASSERT_STREQ( COM_Parse( &p ), "tok", "token before spaced //" );
	ASSERT_STREQ( COM_Parse( &p ), "next", "after line comment" );
	return 0;
}

static int test_parse_empty_quoted(void)
{
	const char *s = "\"\" x";
	const char *p = s;

	COM_BeginParseSession( "test" );
	ASSERT_STREQ( COM_Parse( &p ), "", "empty quotes" );
	ASSERT_STREQ( COM_Parse( &p ), "x", "after empty quotes" );
	return 0;
}

static int test_parse_ext_no_linebreak(void)
{
	const char *s = "first\nsecond";
	const char *p = s;

	COM_BeginParseSession( "test" );
	ASSERT_STREQ( COM_ParseExt( &p, qtrue ), "first", "allow breaks: first" );
	ASSERT_STREQ( COM_ParseExt( &p, qtrue ), "second", "allow breaks: second" );

	p = s;
	COM_BeginParseSession( "test" );
	ASSERT_STREQ( COM_ParseExt( &p, qfalse ), "first", "no breaks: first" );
	/* SkipWhitespace consumes the newline; empty token + cursor at next token */
	ASSERT_STREQ( COM_ParseExt( &p, qfalse ), "", "no breaks: empty at line boundary" );
	ASSERT( p != NULL && strncmp( p, "second", 6 ) == 0, "cursor after nl at next token" );
	ASSERT_STREQ( COM_ParseExt( &p, qfalse ), "second", "no breaks: token after boundary" );
	return 0;
}

static int test_parse_line_numbers(void)
{
	const char *s = "one\ntwo";
	const char *p = s;

	COM_BeginParseSession( "test" );
	(void)COM_Parse( &p );
	ASSERT_EQ( COM_GetCurrentParseLine(), 1, "token on line 1" );
	(void)COM_Parse( &p );
	ASSERT_EQ( COM_GetCurrentParseLine(), 2, "token after newline line 2" );
	return 0;
}

static int test_compress_comments_and_quotes(void)
{
	char buf[128];

	Q_strncpyz( buf, "a //x\nb", sizeof( buf ) );
	ASSERT( COM_Compress( buf ) > 0, "compress length" );
	ASSERT_STREQ( buf, "a\nb", "compress strips // line" );

	Q_strncpyz( buf, "x /*c*/ y", sizeof( buf ) );
	COM_Compress( buf );
	ASSERT_STREQ( buf, "x y", "compress strips block comment" );

	Q_strncpyz( buf, "z \"a b\" w", sizeof( buf ) );
	COM_Compress( buf );
	ASSERT_STREQ( buf, "z \"a b\" w", "compress keeps quoted" );
	return 0;
}

static int test_parse_complex_operators(void)
{
	const char *s = "a == b != c";
	const char *p = s;

	COM_BeginParseSession( "test" );
	ASSERT_STREQ( COM_ParseComplex( &p, qtrue ), "a", "complex a" );
	ASSERT( com_tokentype == TK_STRING, "a type string" );

	ASSERT_STREQ( COM_ParseComplex( &p, qtrue ), "==", "complex ==" );
	ASSERT( com_tokentype == TK_EQ, "== type" );

	ASSERT_STREQ( COM_ParseComplex( &p, qtrue ), "b", "complex b" );

	ASSERT_STREQ( COM_ParseComplex( &p, qtrue ), "!=", "complex !=" );
	ASSERT( com_tokentype == TK_NEQ, "!= type" );

	ASSERT_STREQ( COM_ParseComplex( &p, qtrue ), "c", "complex c" );
	return 0;
}

static int test_parse_complex_newline_token(void)
{
	const char *s = "a\nb";
	const char *p = s;

	COM_BeginParseSession( "test" );
	ASSERT_STREQ( COM_ParseComplex( &p, qfalse ), "a", "complex before nl" );
	ASSERT_STREQ( COM_ParseComplex( &p, qfalse ), "", "newline token empty" );
	ASSERT( com_tokentype == TK_NEWLINE, "TK_NEWLINE when breaks disallowed" );
	return 0;
}

static int test_skip_braced_section(void)
{
	const char *s = "before { inner { x } } after";
	const char *p = s;

	COM_BeginParseSession( "test" );
	ASSERT_STREQ( COM_Parse( &p ), "before", "before brace" );
	ASSERT( SkipBracedSection( &p, 0 ) == qtrue, "skip braced" );
	ASSERT_STREQ( COM_Parse( &p ), "after", "after braced" );
	return 0;
}

static int test_skip_rest_of_line(void)
{
	const char *s = "keep rest\nnext";
	const char *p = s;

	COM_BeginParseSession( "test" );
	ASSERT_STREQ( COM_Parse( &p ), "keep", "token before skip" );
	SkipRestOfLine( &p );
	ASSERT( *p == 'n', "cursor at next line" );
	ASSERT_STREQ( COM_Parse( &p ), "next", "token after skip line" );
	return 0;
}

static int test_skip_rest_of_line_at_eof(void)
{
	const char *s = "only";
	const char *p = s;

	COM_BeginParseSession( "test" );
	SkipRestOfLine( &p );
	ASSERT( *p == '\0', "skip at start eof" );
	return 0;
}

static int test_skip_braced_depth_one(void)
{
	/* depth==1: caller already counted the opening '{'; scan until matching '}' */
	const char *s = "a b } tail";
	const char *p = s;

	COM_BeginParseSession( "test" );
	ASSERT( SkipBracedSection( &p, 1 ) == qtrue, "depth 1 after consuming open brace" );
	ASSERT_STREQ( COM_Parse( &p ), "tail", "after depth-1 skip" );
	return 0;
}

static int test_parse_complex_and_or(void)
{
	const char *s = "a && b || c";
	const char *p = s;

	COM_BeginParseSession( "test" );
	ASSERT_STREQ( COM_ParseComplex( &p, qtrue ), "a", "and-or a" );
	ASSERT_STREQ( COM_ParseComplex( &p, qtrue ), "&&", "&& token" );
	ASSERT( com_tokentype == TK_AND, "TK_AND" );
	ASSERT_STREQ( COM_ParseComplex( &p, qtrue ), "b", "and-or b" );
	ASSERT_STREQ( COM_ParseComplex( &p, qtrue ), "||", "|| token" );
	ASSERT( com_tokentype == TK_OR, "TK_OR" );
	ASSERT_STREQ( COM_ParseComplex( &p, qtrue ), "c", "and-or c" );
	return 0;
}

static int test_parse_null_input(void)
{
	const char *p = NULL;

	COM_BeginParseSession( "test" );
	ASSERT_STREQ( COM_Parse( &p ), "", "NULL in" );
	ASSERT( p == NULL, "NULL out" );
	return 0;
}

int main( void )
{
	if ( test_parse_simple_tokens() ) return 1;
	if ( test_parse_quoted_spaces() ) return 1;
	if ( test_parse_line_comment() ) return 1;
	if ( test_parse_block_comment() ) return 1;
	if ( test_parse_quote_then_comment() ) return 1;
	if ( test_parse_line_comment_after_token() ) return 1;
	if ( test_parse_empty_quoted() ) return 1;
	if ( test_parse_ext_no_linebreak() ) return 1;
	if ( test_parse_line_numbers() ) return 1;
	if ( test_compress_comments_and_quotes() ) return 1;
	if ( test_parse_complex_operators() ) return 1;
	if ( test_parse_complex_newline_token() ) return 1;
	if ( test_skip_braced_section() ) return 1;
	if ( test_skip_braced_depth_one() ) return 1;
	if ( test_skip_rest_of_line() ) return 1;
	if ( test_skip_rest_of_line_at_eof() ) return 1;
	if ( test_parse_complex_and_or() ) return 1;
	if ( test_parse_null_input() ) return 1;
	return 0;
}
