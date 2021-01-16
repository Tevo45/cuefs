#include <u.h>
#include <libc.h>

#include "cuefs.h"

char *fname = "<stdin>";

void
usage(void)
{
	fprint(2, "usage: %s [-Dv] [-m mtpt] [-f outfmt] [-i index] file\n", argv0);
	exits("usage");
}

int
fmtarg(char *fmt)
{
	static char *fmts[] =
	{
		[WAVE]		= "wave",
		[FLAC]		= "flac",
		[BINARY]	= "pcm",
	};

	for(int c = 0; c < sizeof(fmts); c++)
		if(cistrcmp(fmt, fmts[c]) == 0)
			return c;

	return UNKNOWN;
}

void
main(int argc, char **argv)
{
	extern int chatty9p;
	char *mtpt = "/mnt/cue";
	int outfmt = -1, prefidx = 0;

	ARGBEGIN {
	case 'D':
		chatty9p++;
		break;
	case 'm':
		mtpt = EARGF(usage());
		break;
	case 'v':
		verbosity++;
		break;
	case 'f':
		if((outfmt = fmtarg(EARGF(usage()))) == UNKNOWN)
			usage();
		break;
	case 'i':
		prefidx = atoi(EARGF(usage()));
		break;
	default:
		usage();
	} ARGEND;

	if(argc > 1)
		usage();

	if(argc == 1)
	{
		fname = argv[0];
		if((infd = open(fname, OREAD)) < 0)
			sysfatal("open: %r");
	}

	cursheet = newsheet();
	yyparse();

	if(infd != 0)
		close(infd);

	cuefsinit(cursheet, mtpt, outfmt, prefidx);

	exits(0);
}
