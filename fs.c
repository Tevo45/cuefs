#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

#include "cuefs.h"

typedef struct {
	Cuesheet* sheet;
	int outfmt;
} Fsprops;

void wavserve(Entry*, Req*);

void (*servefmt[])(Entry*, Req*) =
{
	[WAVE]	= wavserve,

	[UNKNOWN] = nil
};

char *decoder[] =
{
	[MP3]	= "audio/mp3dec",
	[FLAC]	= "audio/flacdec",
	[WAVE]	= "audio/wavedec"
};

int
pipedec(AFile *f, Timestamp t)
{
	int fd[2];
	double sec;
	char *dec;

	dec = decoder[f->actual];
	sec = t2sec(t);

	if(pipe(fd) < 0)
		sysfatal("pipedec: can't decode: pipe: %r");

	switch(rfork(RFFDG|RFPROC|RFMEM|RFNAMEG|RFNOTEG|RFREND))
	{
	case 0:
		close(0);
		close(1);
		dup(f->fd, 0);
		dup(fd[1], 1);
		close(f->fd);
		close(fd[1]);
		dec = strdup(dec);
		{
			char *argv[] = { dec };
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
	return fd[0];
}

void
wavserve(Entry *e, Req *r)
{
	int dec;

	dec = pipedec(e->file, e->starts[0]);
	r->ofcall.count = readn(dec, r->ofcall.data, r->ifcall.count);
	respond(r, Estub);
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
	p->outfmt = WAVE;	/* STUB */

	fs.aux	= p;
	fs.tree	= alloctree(nil, nil, DMDIR | 0444, nil);

	for(AFile *f = sheet->files; f != nil; f = f->next)
		if(f->fd = open(f->name, OREAD) < 0)
			sysfatal("open: %r");

	for(Entry *e = sheet->entries; e != nil; e = e->next)
	{
		s = smprint("%02d - %s.%s", e->index, e->title, formatext(e->file));
		strreplace(s, '/', '-');
		createfile(fs.tree->root, s, nil, 0444, e);
		free(s);
	}

	postmountsrv(&fs, nil, mtpt, MREPL);
}
