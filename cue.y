%{
#include <u.h>
#include <libc.h>

#include "cuefs.h"

/*
 * FIXME find a way to "fix" the grammar so that it
 * doesn't do right-hand recursion (and overflow the
 * stack) instead of just growing it and pretending
 * it's not a problem
 */
#define YYMAXDEPTH 8192
%}

%union
{
	Timestamp time;
	char *str;
	int i;
}

%token <i>   INTEGER
%token <str> STRING

%type <i>    filetype
%type <time> timestamp

%token CATALOG CDTEXTFILE FLAGS DCP CHAN4 PREEMPH SCMS INDEX
%token ISRC PERFORMER POSTGAP PREGAP SONGWRITER TITLE TRACK
%token FILE FWAVE FMP3 FAIFF FBINARY FMOTOROLA AUDIO

%%
cuesheet:
	| expr '\n' cuesheet
	;

expr:
	| PERFORMER STRING			{ setperformer(cursheet, $2); }
	| TITLE STRING				{ settitle(cursheet, $2); }
	| FILE STRING filetype		{ addfile(cursheet, $2, $3); }
	| TRACK INTEGER AUDIO		{ addnewtrack(cursheet, $2); }
	| INDEX INTEGER timestamp	{ settimestamp(cursheet, $2, $3); }
	;

filetype:
	FWAVE			{ $$ = WAVE; }
	| FMP3			{ $$ = MP3; }
	| FAIFF			{ $$ = AIFF; }
	| FBINARY		{ $$ = BINARY; }
	| FMOTOROLA		{ $$ = MOTOROLA; }
	;

timestamp:
	INTEGER ':' INTEGER ':' INTEGER		{ $$ = parsetime($1, $3, $5); }
	;
%%
