%{
#include <u.h>
#include <libc.h>

#include "cuefs.h"

Cuesheet *cursheet;

/* maybe it doesn't matter */
#define YYMAXDEPTH 8192
%}

%union
{
	Timestamp time;
	char *str;
	int i;
}

%token <i>   INTEGER
%token <str> STRING MCN ISRCCODE

%type <i>    filetype
%type <time> timestamp
%type <i> flag flags

%token CATALOG CDTEXTFILE FLAGS DCP CHAN4 PREEMPH SCMS INDEX
%token ISRC PERFORMER POSTGAP PREGAP SONGWRITER TITLE TRACK
%token FILE FWAVE FMP3 FAIFF FBINARY FMOTOROLA AUDIO MCN ISRCCODE

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
	| CATALOG MCN				{ setmcn(cursheet, $2); }
	| FLAGS flags				{ setflags(cursheet, $2); }
	| ISRC ISRCCODE			{ setisrc(cursheet, $2); }
	| PREGAP timestamp			{ setpregap(cursheet, $2); }
	| POSTGAP timestamp		{ setpostgap(cursheet, $2); }
	| SONGWRITER STRING		{ setsongwriter(cursheet, $2); }
	;

flags:						{ $$ = 0; }
	| flags flag				{ $$ = $1 | $2; }
	;

flag:
	DCP						{ $$ = FLAG_DCP; }
	| CHAN4					{ $$ = FLAG_4CH; }
	| PREEMPH				{ $$ = FLAG_PRE; }
	| SCMS					{ $$ = FLAG_SCMS; }
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
