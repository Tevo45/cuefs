#include <u.h>
#include <libc.h>

#include "cuefs.h"

char *fname = "<stdin>";

void
usage(void)
{
	fprint(2, "usage: %s\n", argv0);
	exits("usage");
}

void
main(int argc, char **argv)
{
	ARGBEGIN {
	default:
		usage();
	} ARGEND;

	if(argc > 1)
		usage();

	if(argc == 1)
	{
		fname = argv[0];
		infd = open(fname, OREAD);
	}

	cursheet = newsheet();
	yyparse();

	close(infd);

	exits(0);
}
