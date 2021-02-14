#include <u.h>
#include <libc.h>

#include "cuefs.h"
#include "y.tab.h"

/*
 * minute:second:frames → Timestamp
 * 1 frame = 1/75 second
 */
Timestamp
parsetime(int min, int sec, int frames)
{
	sec += min*60;
	frames += sec*75;
	return (Timestamp){frames};
}

double
t2sec(Timestamp t)
{
	return t.frames/75.0;
}

/*
 * offset (in bytes) → sec, with 
 * rate = sample rate, size = sample size (bits), chans = channels
 */
double
of2sec(uint rate, uint size, uint chans, vlong offset)
{
	int bs;
	bs = (size/8)*chans;
	return (double)offset/bs/rate;
}

Cuesheet*
newsheet(void)
{
	return emallocz(sizeof(Cuesheet), 1);
}

void
freesheet(Cuesheet *s)
{
	Entry *e;
	AFile *f;
	Start *st;

	/* free entries */
	while((e = s->entries) != nil)
	{
		s->entries = e->next;

		/* free starts */
		while((st = e->starts) != nil)
		{
			e->starts = st->next;
			free(st);
		}
		free(e->title);
		free(e->isrc);
		/*
		 * TODO maybe we should keep track of every performer in
		 * the Cuesheet, even if they're "song only" performers?
		 */
		if(e->performer != s->performer)
			free(e->performer);

		free(e);
	}

	/* free files */
	while((f = s->files) != nil)
	{
		s->files = f->next;

		free(f->name);
		free(f);
	}

	free(s->title);
	free(s->performer);
	free(s->mcn);

	free(s);
}

AFile*
lastfile(Cuesheet *c)
{
	if(c->files == nil)
		return nil;

	if(c->curfile == nil)
		c->curfile = c->files;

	while(c->curfile->next != nil)
		c->curfile = c->curfile->next;

	return c->curfile;
}

Entry*
lastentry(Cuesheet *c)
{
	if(c->entries == nil)
		return nil;

	if(c->curentry == nil)
		c->curentry = c->entries;

	while(c->curentry->next != nil)
		c->curentry = c->curentry->next;

	return c->curentry;
}

void
setperformer(Cuesheet *c, char *artist)
{
	artist = strdup(artist);
	if(c->curentry == nil)
	{
		free(c->performer);
		c->performer = artist;
	}
	else
	{
		if(c->curentry->performer != c->performer)
			free(c->curentry->performer);
		c->curentry->performer = artist;
	}
}

void
settitle(Cuesheet *c, char *title)
{
	title = strdup(title);
	if(c->curentry == nil)
	{
		free(c->title);
		c->title = title;
	}
	else
	{
		free(c->curentry->title);
		c->curentry->title = title;
	}
}

void
setmcn(Cuesheet *c, char *mcn)
{
	c->mcn = mcn;
}

void
setflags(Cuesheet *c, int flags)
{
	if(c->curentry == nil)
		parserfatal("flag outside of track");
	c->curentry->flags = flags;
}

void
setisrc(Cuesheet *c, char *isrc)
{
	if(c->curentry == nil)
		parserfatal("flag outside of track");
	c->curentry->isrc = isrc;
}

void
addfile(Cuesheet *c, char *name, int format)
{
	AFile *new;

	new = emallocz(sizeof(*new), 1);
	new->name	= strdup(name);
	new->type	= format;
	new->actual	= actualformat(new);
	new->next	= nil;

	if(c->files == nil)
		c->files = new;
	else
		lastfile(c)->next = new;

	c->curfile = new;
}

void
addnewtrack(Cuesheet *c, int i)
{
	Entry *new;

	new = emallocz(sizeof(*new), 1);
	new->sheet = c;
	new->file = c->curfile;
	new->performer = c->performer;
	new->index = i;

	if(c->entries == nil)
		c->entries = new;
	else
		lastentry(c)->next = new;

	c->curentry = new;
}

void
settimestamp(Cuesheet *c, int i, Timestamp t)
{
	Start *entry, **p;

	if(c->curentry == nil)
		parserfatal("timestamp outside of track");

	debug("setting timestamp[%d] for %d as %ud frames\n", i, c->curentry->index, t.frames);

	entry = emallocz(sizeof(*entry), 1);
	entry->Timestamp = t;
	entry->index = i;

	p = &c->curentry->starts;

	while((*p) && (*p)->index < i)
		p = &(*p)->next;

	entry->next = *p;
	*p = entry;
}

char*
extension(char *f)
{
	char *ext = nil;

	for(; *f != 0; f++)
		if(*f == '.')
			ext = f+1;

	return ext;
}

int
actualformat(AFile *f)
{
	char *ext;

	if(f->type != WAVE)
		return f->type;

	if((ext = extension(f->name)) == nil)
		return UNKNOWN;

	if(strcmp(ext, "wav") == 0)
		return WAVE;
	if(strcmp(ext, "flac") == 0 || strcmp(ext, "fla") == 0)
		return FLAC;

	return UNKNOWN;
}

char*
formatext(int f)
{
	static char *tab[] =
	{
		[WAVE]		= "wav",
		[MP3]		= "mp3",
		[AIFF]		= "aiff",
		[BINARY]	= "bin",
		[FLAC]		= "flac",
		[OGG]		= "ogg",
		[OPUS]		= "opus",
		[AAC]		= "aac",
		[MOTOROLA]	= "bin"
	};

	return tab[f];
}

char*
fileext(AFile *f)
{
	if(f->actual != UNKNOWN)
		return formatext(f->actual);

	return extension(f->name);
}

int
prefoutfmt(int fmt)
{
	static int self[] = { FLAC };

	for(int c = 0; c < sizeof(self); c++)
		if(fmt == self[c])
			return fmt;

	return WAVE;
}
