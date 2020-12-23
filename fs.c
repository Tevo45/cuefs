#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

#include "cuefs.h"

typedef struct
{
	Cuesheet* sheet;
	int outfmt;
} Fsprops;

void pcmserve(Entry*, Req*);

void (*servefmt[])(Entry*, Req*) =
{
//	[WAVE]		= wavserve,
	[BINARY]	= pcmserve,

	[UNKNOWN] = nil
};

char *decoder[] =
{
	[MP3]	= "audio/mp3dec",
	[FLAC]	= "audio/flacdec",
	[WAVE]	= "audio/wavdec"
};

int
pipedec(AFile *f, double sec)
{
	int fd[2], afd;
	char *dec;

	dec = decoder[f->actual];

	debug("decoding %s starting at %f\n", f->name, sec);

	if(pipe(fd) < 0)
		sysfatal("pipedec: can't decode: pipe: %r");

	switch(rfork(RFPROC|RFFDG|RFREND|RFNOTEG))
	{
	case 0:
		if((afd = open(f->name, OREAD)) < 0)
			sysfatal("pipedec: can't decode: open: %r");
		dup(afd, 0);
		dup(fd[1], 1);
		close(afd);
		close(fd[1]);
		seek(0, 0, 0);
		{
			char *argv[] = { dec, "-s", smprint("%f", sec), nil };
 			debug("command line: ");
			for(char **a = argv; *a != nil; a++)
				debug("'%s' ", *a);
			debug("\n");
			if(argv[2] == nil)
				sysfatal("pipedec: can't decode: smprint: %r");
			exec(dec, argv);
			dec = smprint("/bin/%s", dec);
			if(dec == nil)
				sysfatal("pipedec: can't decode: smprint: %r");
			exec(dec, argv);
			sysfatal("pipedec: can't decode: exec: %r");
		}
		break;
	case -1:
		sysfatal("pipedec: can't decode: rfork: %r");
	}
	close(fd[1]);
	return fd[0];
}

void
pcmserve(Entry *e, Req *r)
{
	double sec;
	int dec;

	sec  = t2sec(e->starts[0]);
	sec += of2sec(44100, 16, 2, r->ifcall.offset);

	dec = pipedec(e->file, sec);
	r->ofcall.count = read(dec, r->ofcall.data, r->ifcall.count);
	close(dec);
	respond(r, nil);
}

void
fsopen(Req *r)
{
	respond(r, nil);
}

void
fsread(Req *r)
{
	void (*func)(Entry*, Req*);
	extern Srv fs;
	Fsprops *p;

	p = fs.aux;

	func = servefmt[p->outfmt];

	if(func != nil)
		func(r->fid->file->aux, r);
	else
		respond(r, Eunsupported);
}

void
fsend(Srv *s)
{
	Fsprops *p;

	p = s->aux;
	freesheet(p->sheet);
	free(p);
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
	Fsprops *p;
	char *s;

	p = emalloc(sizeof(*p));
	p->sheet  = sheet;
	p->outfmt = BINARY;	/* STUB */

	fs.aux	= p;
	fs.tree	= alloctree(nil, nil, DMDIR | 0444, nil);

	for(Entry *e = sheet->entries; e != nil; e = e->next)
	{
		debug("%d: %d\n",  e->index, e->starts[0].frames);
		s = smprint("%02d - %s.%s", e->index, e->title, formatext(p->outfmt));
		strreplace(s, '/', '-');
		createfile(fs.tree->root, s, nil, 0444, e);
		free(s);
	}

	postmountsrv(&fs, nil, mtpt, MREPL);
}
