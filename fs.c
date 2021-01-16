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
	int outfmt, prefidx;
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

typedef struct
{
	Resource;
	Decoder *dec;
	int pollpid, encpid, fd;
	vlong curoff;
} Flacenc;

void pcmserve(Entry*, Req*);
void wavserve(Entry*, Req*);
void flacserve(Entry*, Req*);

void (*servefmt[])(Entry*, Req*) =
{
	[WAVE]		= wavserve,
	[FLAC]		= flacserve,
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
kill(int pid)
{
	char *path;
	int fd;

	if((path = smprint("/proc/%d/notepg", pid)) == nil)
		sysfatal("smprint: %r");
	if((fd = open(path, OWRITE)) < 0)
		goto end;
	write(fd, "kill", strlen("kill"));
	close(fd);
end:
	free(path);
}

Timestamp*
prefindex(Entry *e)
{
	extern Srv fs;
	Fsprops *p;

	p = fs.aux;

	for(Start *s = e->starts; s != nil; s = s->next)
		if(s->index == p->prefidx)
			return s;

	return e->starts;
}

void
closedec(Decoder *dec)
{
	if(dec == nil)
		return;

	close(dec->fd);
	kill(dec->pid);
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
	int fd[2], afd, devnull = -1;
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
		if((afd = open(f->name, OREAD)) < 0 || (devnull = open("/dev/null", OWRITE)) < 0)
			sysfatal("pipedec: can't decode: open: %r");
		dup(afd, 0);
		dup(fd[1], 1);
		if(verbosity < 1)
			dup(devnull, 2);
		close(afd);
		close(fd[1]);
		close(devnull);
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

	debug("readdec: reading %ld bytes from pid %d\n", count, dec->pid);

	ret = read(dec->fd, buf, count);
	dec->curoff += ret;

	return ret;
}

vlong
entrylen(Entry *e)
{
	vlong end;

	if(e->next != nil)
	{
		/* amount of samples between songs... */
		end = (prefindex(e->next)->frames - prefindex(e)->frames) * (44100/75);
		/* ...*2 channels, 2 bytes per sample */
		end *= 2*2;
	}
	else
		end = 0;

	return end;
}

Decoder*
reqdec(Entry *e, Req *r, ulong offset)
{
	Decoder *dec;
	double sec;

	sec = t2sec(*prefindex(e)) + of2sec(44100, 16, 2, offset);

	/*
	 * wouldn't be that bad to just read and throw away a little of the
	 * decoded pcm if offset isn't that far from dec->curoff
	 */
	if((dec = r->fid->aux) == nil || dec->curoff != offset)
	{
		closedec(dec);
		dec = r->fid->aux = pipedec(e->file, sec, offset, entrylen(e));
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
closeflac(Flacenc *enc)
{
	if(enc == nil)
		return;

	closedec(enc->dec);
	kill(enc->pollpid);
	kill(enc->encpid);
	close(enc->fd);

	free(enc);
}

int
polldec(Decoder *dec, int fd)
{
	char buf[4096] = {0};
	int pid;

	switch(pid = rfork(RFPROC|RFFDG|RFREND|RFNOTEG))
	{
	case 0:
		for(int n = -1; n != 0;)
		{
			debug("polldec: reading %d from decoder\n", sizeof(buf));
			n = readdec(dec, buf, sizeof(buf));
			write(fd, buf, sizeof(buf));
			debug("polldec: writing %d into poll pipe\n", n);
		}
		debug("polldec: decoder finished, exiting\n");
		closedec(dec);
		exits(0);
	case -1:
		sysfatal("polldec: rfork: %r");
	}

	close(fd);
	return pid;
}

int
_flacenc(Entry *e, int infd, int outfd)
{
	static char *enc = "audio/flacenc";
	int pid;

	switch(pid = rfork(RFPROC|RFFDG|RFREND|RFNOTEG))
	{
	case 0:
		dup(infd, 0);
		dup(outfd, 1);
		close(infd);
		close(outfd);
		{
			/* TODO better metadata handling */
			char *argv[] =
			{ 
				enc, 
				"-T", smprint("TITLE=%s", e->title),
				"-T", smprint("ARTIST=%s", e->performer),
				"-T", smprint("ALBUMARTIST=%s", e->sheet->performer),
				"-T", smprint("ALBUM=%s", e->sheet->title),
				nil
			};
			exec(enc, argv);
			enc = smprint("/bin/%s", enc);
			if(enc == nil)
				sysfatal("_flacenc: can't encode: smprint: %r");
			exec(enc, argv);
			sysfatal("_flacenc: can't encode: exec: %r");
		}
	case -1:
		sysfatal("_flacenc: can't encode: rfork: %r");
	}

	close(infd);
	close(outfd);
	return pid;
}

Flacenc*
flacenc(Entry *e)
{
	Flacenc *enc;
	int encfd[2], decfd[2];

	if(pipe(encfd) < 0 || pipe(decfd) < 0)
		sysfatal("flacenc: pipe: %r");

	enc = emalloc(sizeof(*enc));
	enc->cleanup = (void(*)(void*))closeflac;
	enc->fd = encfd[0];
	enc->dec = pipedec(e->file, t2sec(*prefindex(e)), 0, entrylen(e));
	enc->pollpid = polldec(enc->dec, decfd[1]);
	enc->encpid = _flacenc(e, decfd[0], encfd[1]);

	return enc;
}

long
readflac(Flacenc *enc, void *buf, long count)
{
	long ret;

	debug("readflac: reading %ld bytes from poll pipe\n", count);
	ret = read(enc->fd, buf, count);
	enc->curoff += ret;

	return ret;
}

long
seekflac(Flacenc *enc, vlong offset)
{
	char buf[4096];

	if(offset < enc->curoff)
		return enc->curoff;

	debug("seekflac: %lld → %lld\n", offset);

	for(int todo; (todo = enc->curoff - offset) == 0;)
	{
		debug("seekflac: %d to go\n");
		if(readflac(enc, buf, todo < sizeof(buf) ? todo : sizeof(buf)) == 0)
			break;
	}

	return enc->curoff;
}

void
flacserve(Entry *e, Req *r)
{
	Flacenc *enc;

	if((enc = r->fid->aux) == nil || enc->curoff < r->ifcall.offset)
	{
		closeflac(enc);
		enc = r->fid->aux = flacenc(e);
	}

	if(enc->curoff != r->ifcall.offset)
		seekflac(enc, r->ifcall.offset);

	r->ofcall.count = readflac(enc, r->ofcall.data, r->ifcall.count);
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
cuefsinit(Cuesheet *sheet, char *mtpt, int outfmt, int prefidx)
{
	Fsprops *p;
	char *s;

	p = emalloc(sizeof(*p));
	p->sheet	= sheet;
	p->outfmt	= (outfmt == -1) ? prefoutfmt(sheet->files->actual) : outfmt;
	p->prefidx	= prefidx;

	fs.aux	= p;
	fs.tree	= alloctree(nil, nil, DMDIR | 0444, nil);

	for(AFile *file = sheet->files; file != nil; file = file->next)
	{
		if(access(file->name, AREAD) < 0)
			sysfatal("cannot access: %r");

		if(access(s = decoder[file->actual], AEXEC) < 0)
		{
			if(access(s = smprint("/bin/%s", s), AEXEC) < 0)
				sysfatal("cannot find decoder: %r");
			free(s);
		}
	}

	for(Entry *e = sheet->entries; e != nil; e = e->next)
	{
		debug("%d: %d\n",  e->index, prefindex(e)->frames);
		/* TODO make the format customizable */
		s = smprint("%02d - %s.%s", e->index, e->title, formatext(p->outfmt));
		strreplace(s, '/', '-');
		createfile(fs.tree->root, s, nil, 0444, e);
		free(s);
	}

	postmountsrv(&fs, nil, mtpt, MREPL);
}
