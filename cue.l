%{
#include "cuefs.h"
#include "y.tab.h"

%}

%%
\".*\"		{
					yylval.str = strdup(yytext+1);

					if(yylval.str[yyleng-2] != '"')
						parserwarn("improperly terminated string");
					else
						yylval.str[yyleng-2] = '\0';

					return STRING;
				}

[0-9]+			{
					yylval.i = atoi(yytext);
					return INTEGER;
				}

"CATALOG"		return CATALOG;
"CDTEXTFILE"	return CDTEXTFILE;
"FILE"			return FILE;
"FLAGS"			return FLAGS;
"DCP"			return DCP;
"4CH"			return CHAN4;
"PRE"			return PREEMPH;
"SCMS"			return SCMS;
"INDEX"			return INDEX;
"ISRC"			return ISRC;
"PERFORMER"		return PERFORMER;
"POSTGAP"		return POSTGAP;
"PREGAP"		return PREGAP;
"SONGWRITER"	return SONGWRITER;
"TITLE"			return TITLE;
"TRACK"			return TRACK;
"WAVE"			return FWAVE;
"MP3"			return FMP3;
"AIFF"			return FAIFF;
"BINARY"		return FBINARY;
"MOTOROLA"		return FMOTOROLA;
"AUDIO"			return AUDIO;

[:\n]			return *yytext;

"REM".*\n		;
[ \t\r]+		;
%%
