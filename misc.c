#include <u.h>
#include <libc.h>

#include "cuefs.h"
#include "y.tab.h"

int verbosity = 0;

void*
erealloc(void *p, ulong s)
{
	p = realloc(p, s);
	if(s > 0 && p == nil)
		sysfatal("realloc: %r");
	setrealloctag(p, getcallerpc(&p));
	return p;
}

void*
emallocz(ulong s, int clr)
{
	void* p = mallocz(s, clr);
	if(p == nil)
		sysfatal("mallocz: %r");
	setmalloctag(p, getcallerpc(&s));
	return p;
}

void*
emalloc(ulong s)
{
	void* p = malloc(s);
	if(p == nil)
		sysfatal("malloc: %r");
	setmalloctag(p, getcallerpc(&s));
	return p;
}

char*
esmprint(char *fmt, ...)
{
	char *str;
	va_list args;

	va_start(args, fmt);
	str = vsmprint(fmt, args);
	if(str == nil)
		sysfatal("vsmprint: %r");
	va_end(args);

	return str;
}

/* it's a mess */

void
yywarn(char *str)
{
	extern int yylineno;
	fprint(2, "%s:%d: %s\n", fname, yylineno, str);
}

void
yyerror(char *str)
{
	yywarn(str);
	exits(str);
}

char*
setstr(char *baseline, char **dest, char *str)
{
	if(*dest != nil && *dest != baseline)
		free(*dest);
	*dest = str;
	return str;
}

char*
strreplace(char *str, char a, char b)
{
	for(char *c = str; *c != 0; c++)
		if(*c == a)
			*c = b;
	return str;
}

void
parserwarn(char *fmt, ...)
{
	char *str;
	va_list args;

	va_start(args, fmt);
	str = vsmprint(fmt, args);
	yywarn(str);
	free(str);
	va_end(args);
}

void
parserfatal(char *fmt, ...)
{
	char *str;
	va_list args;

	va_start(args, fmt);
	str = vsmprint(fmt, args);
	yywarn(str);
	free(str);
	va_end(args);
	exits("cantparse");
}

void
debug(char *fmt, ...)
{
	va_list args;

	if(verbosity < 3)
		return;

	va_start(args, fmt);
	vfprint(2, fmt, args);
	va_end(args);
}
