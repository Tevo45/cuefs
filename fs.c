#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

#include "cuefs.h"

typedef struct
{
	void (*cleanup)(void*);
} Resource;

typedef struct
{
	Cuesheet* sheet;
	int outfmt;
} Fsprops;

typedef struct
{
	Resource;
	int fd, pid;
	vlong curoff, end;
} Decoder;

/* from http://soundfile.sapp.org/doc/WaveFormat/ */

/* TODO maybe this should be packed on some architectures? */
typedef struct
{
		/* RIFF header */
	u32int id;			/* BE */	/* RIFF */
	u32int size;		/* LE */
	u32int fmt;			/* BE */	/* WAVE */
		/* fmt section */
	u32int fmtid;		/* BE */	/* fmt  */
	u32int fmtsize;		/* LE */
	u16int format;		/* LE */
	u16int chans;		/* LE */
	u32int srate;		/* LE */
	u32int brate;		/* LE */
	u16int ballign;		/* LE */
	u16int ssize;		/* LE */
		/* data section */
	u32int dataid;		/* BE */	/* data */
	u32int datasize;	/* LE */
} Wavehdr;

void pcmserve(Entry*, Req*);
void wavserve(Entry*, Req*);

void (*servefmt[])(Entry*, Req*) =
{
	[WAVE]		= wavserve,
	[BINARY]	= pcmserve,

	[UNKNOWN] = nil
};

char *decoder[] =
{
	[OGG]	= "audio/oggdec",
	[MP3]	= "audio/mp3dec",
	[AAC]	= "audio/aacdec",	/* ↓ */
	[OPUS]	= "audio/opusdec",	/* might not exist */
	[FLAC]	= "audio/flacdec",
	[WAVE]	= "audio/wavdec",
};

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
		goto cleanup;	/* open failed, assume it's already dead */
	write(fd, "kill", strlen("kill"));
	close(fd);

cleanup:
	free(path);
	free(dec);
}

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
	ret->cleanup = (void(*)(void*))closedec;
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

long
readdec(Decoder *dec, void *buf, long count)
{
	long ret, n;

	debug("readdec: decoder offset = %lld, decoder end = %lld, count = %ld\n",
		dec->curoff, dec->end, count);

	/* dec->end == 0 means "there's no end, read what you can" */
	if(dec->end > 0)
	{
		n = dec->end - dec->curoff + count;
		if(n < 0)
			count += n;
	}

	debug("reading %ld bytes from pid %d\n", count, dec->pid);

	ret = read(dec->fd, buf, count);
	dec->curoff += ret;

	return ret;
}

Decoder*
reqdec(Entry *e, Req *r, ulong offset)
{
	Decoder *dec;
	double sec;
	ulong end;

	sec = t2sec(e->starts[0]) + of2sec(44100, 16, 2, offset);

	/*
	 * wouldn't be that bad to just read and throw away a little of the
	 * decoded pcm if offset isn't that far from dec->curoff
	 */
	if((dec = r->fid->aux) == nil || dec->curoff != offset)
	{
		if(e->next != nil)
		{
			/* amount of samples between songs... */
			end = (e->next->starts->frames - e->starts->frames) * (44100/75);
			/* ...*2 channels, 2 bytes per sample */
			end *= 2*2;
		}
		else
			end = 0;
		closedec(dec);
		dec = r->fid->aux = pipedec(e->file, sec, offset, end);
	}

	return dec;
}

void
pcmserve(Entry *e, Req *r)
{
	Decoder *dec;

	dec = reqdec(e, r, r->ifcall.offset);
	r->ofcall.count = readdec(dec, r->ofcall.data, r->ifcall.count);
	respond(r, nil);
}

u32int
s2i(char *s)
{
	return (s[3] << 24 | s[2] << 16 | s[1] << 8 | s[0]);
}

void
fillwavehdr(Wavehdr *hdr, u16int chans, u32int srate, u16int ssize, u32int dsize)
{
	hdr->id		= s2i("RIFF");
	hdr->size	= 36 + dsize;
	hdr->fmt	= s2i("WAVE");

	hdr->fmtid		= s2i("fmt ");
	hdr->fmtsize	= 16;
	hdr->format		= 1;	/* raw pcm */
	hdr->chans		= chans;
	hdr->srate		= srate;
	hdr->brate		= srate * chans * ssize/8;
	hdr->ssize		= ssize;

	hdr->dataid		= s2i("data");
	hdr->datasize	= dsize;
}

void
wavserve(Entry *e, Req *r)
{
	Wavehdr hdr;
	Decoder *dec;
	ulong offset, count, hcount;

	offset = r->ifcall.offset;
	count = r->ifcall.count;

	hcount = 0;

	/* 44 == start of pcm data */
	if(offset < 44)
	{
		hcount = 44 - offset;
		count -= hcount;
		offset = 0;
		fillwavehdr(&hdr, 2, 44100, 16, count);
		memcpy(r->ofcall.data, &hdr, hcount);
	}

	if(count == 0)
	{
		respond(r, nil);
		return;
	}

	dec = reqdec(e, r, offset);
	r->ofcall.count = readdec(dec, r->ofcall.data+hcount, count);
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
	Resource *res;

	if((res = fid->aux) != nil)
		res->cleanup(res);
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
	p->outfmt = WAVE;	/* STUB */

	fs.aux	= p;
	fs.tree	= alloctree(nil, nil, DMDIR | 0444, nil);

	/* TODO check if decoder, encoder, files exist */

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
