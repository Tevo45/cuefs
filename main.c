#include <u.h>
#include <libc.h>

#include "cuefs.h"

char *fname = "<stdin>";

void
usage(void)
{
	fprint(2, "usage: %s [-D] [-m mtpt] file\n", argv0);
	exits("usage");
}

void
main(int argc, char **argv)
{
	extern int chatty9p;
	char *mtpt = "/n/cue";

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

	cuefsinit(cursheet, mtpt);

	exits(0);
}
