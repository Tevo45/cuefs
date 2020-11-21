#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

#include "cuefs.h"
#include "y.tab.h"

char *Estub = "not yet";

Cuesheet *cursheet;

void
fsopen(Req *r)
{
	respond(r, nil);
}

void
fsread(Req *r)
{
	respond(r, Estub);
}

void
fsend(Srv *s)
{
	freesheet(s->aux);
}

Srv fs =
{
	.open	= fsopen,
	.read	= fsread,

	.end	= fsend
};

void
cuefsinit(Cuesheet *sheet, char *mtpt)
{
	char *s;

	fs.aux	= sheet;
	fs.tree	= alloctree(nil, nil, DMDIR | 0444, nil);

	for(Entry *e = sheet->entries; e != nil; e = e->next)
	{
		s = smprint("%02d - %s.%s", e->index, e->title, formatext(e->file));
		createfile(fs.tree->root, s, nil, 0444, e);
		free(s);
	}

	postmountsrv(&fs, nil, mtpt, MREPL);
}
