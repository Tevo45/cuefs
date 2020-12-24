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

typedef struct
{
	int fd, pid;
	vlong curoff, end;
} Decoder;

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

/* 
 * FIXME find a better way to signal decoder failure,
 * one that we can answer the Tread with
 */
Decoder*
pipedec(AFile *f, double sec, vlong off, vlong end)
{
	Decoder *ret;
	int fd[2], afd;
	char *dec;

	dec = decoder[f->actual];

	debug("decoding %s starting at %f\n", f->name, sec);

	if(pipe(fd) < 0)
		sysfatal("pipedec: can't decode: pipe: %r");

	ret = emalloc(sizeof(*ret));
	ret->fd = fd[0];
	ret->curoff = off;
	ret->end = end;

	switch(ret->pid = rfork(RFPROC|RFFDG|RFREND|RFNOTEG))
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
	return ret;
}

void
closedec(Decoder *dec)
{
	char *path;
	int fd;

	if(dec == nil)
		return;

	close(dec->fd);

	if((path = smprint("/proc/%d/notepg", dec->pid)) == nil)
		sysfatal("smprint: %r");
	if((fd = open(path, OWRITE)) < 0)
		sysfatal("open: %r");
	write(fd, "kill", strlen("kill"));
	close(fd);

	free(dec);
}

long
readdec(Decoder *dec, void *buf, long count)
{
	long ret, n;

	debug("readdec: decoder offset = %lld, decoder end = %lld, count = %ld\n",
		dec->curoff, dec->end, count);

	n = dec->end - dec->curoff + count;
	if(n < 0)
		count += n;

	debug("reading %ld bytes from pid %d\n", count, dec->pid);

	ret = read(dec->fd, buf, count);
	dec->curoff += ret;

	return ret;
}

void
pcmserve(Entry *e, Req *r)
{
	Decoder *dec;
	double sec;
	long end;

	sec = t2sec(e->starts[0]) + of2sec(44100, 16, 2, r->ifcall.offset);

	/*
	 * wouldn't be that bad to just read and throw away a little of the
	 * decoded pcm if r->ifcall.offset isn't that far from dec->curoff
	 */
	if((dec = r->fid->aux) == nil || dec->curoff != r->ifcall.offset)
	{
		/* amount of samples between songs... */
		end = (e->next->starts->frames - e->starts->frames) * (44100/75);
		/* ...*2 channels, 2 bytes per sample */
		end *= 2*2;
		closedec(dec);
		dec = r->fid->aux = pipedec(e->file, sec, r->ifcall.offset, end);
	}

	r->ofcall.count = readdec(dec, r->ofcall.data, r->ifcall.count);
	respond(r, nil);
}

void
fsopen(Req *r)
{
	respond(r, nil);
}

void
fsclose(Fid *fid)
{
	if(fid->aux != nil)
		closedec(fid->aux);
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
	.open		= fsopen,
	.read		= fsread,
	.destroyfid	= fsclose,

	.end		= fsend
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
		/* TODO make the format customizable */
		s = smprint("%02d - %s.%s", e->index, e->title, formatext(p->outfmt));
		strreplace(s, '/', '-');
		createfile(fs.tree->root, s, nil, 0444, e);
		free(s);
	}

	postmountsrv(&fs, nil, mtpt, MREPL);
}
