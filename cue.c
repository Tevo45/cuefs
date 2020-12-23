#include <u.h>
#include <libc.h>

#include "cuefs.h"
#include "y.tab.h"

Timestamp
parsetime(int min, int sec, int frames)
{
	debug("parsing %d:%d:%d into ", min, sec, frames);
	sec += min*60;
	frames += sec*75;
	debug("%d frames\n", frames);
	return (Timestamp){frames};
}

double
t2sec(Timestamp t)
{
	debug("returning %fs for %ud frames\n", t.frames/75.0, t.frames);
	return t.frames/75.0;
}

double
of2sec(uint rate, uint size, uint chans, vlong offset)
{
	int bs;
	bs = (size/8)*chans;
	return (double)offset/(double)bs/(double)rate;
}

Cuesheet*
newsheet(void)
{
	return emallocz(sizeof(Cuesheet), 1);
}

void
maybefree(void *s, void *p)
{
	if(p != nil && p != s)
		free(p);
}

void
recfreeentries(Cuesheet *s, Entry *cur)
{
	if(cur == nil)
		return;
	recfreeentries(s, cur->next);
	maybefree(nil, cur->starts);
	/* file should get freed by the cuesheet */
	maybefree(nil, cur->title);
	maybefree(s->performer, cur->performer);
}

void
recfreefiles(Cuesheet *s, AFile *cur)
{
	if(cur == nil)
		return;
	recfreefiles(s, cur->next);
	maybefree(nil, cur->name);
}

void
freesheet(Cuesheet *s)
{
	recfreeentries(s, s->entries);
	recfreefiles(s, s->files);
	maybefree(nil, s->title);
	maybefree(nil, s->performer);
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
atleast(Timestamps *ts, int val)
{
	if(ts->maxindex > val)
		return;
	ts->starts = erealloc(ts->starts, (val+1) * sizeof(*ts->starts));
	ts->maxindex = val;
}

void
setperformer(Cuesheet *c, char *artist)
{
	artist = strdup(artist);
	if(c->curentry == nil)
		setstr(nil, &c->performer, artist);
	else
		setstr(c->performer, &c->curentry->performer, artist);
}

void
settitle(Cuesheet *c, char *title)
{
	title = strdup(title);
	if(c->curentry == nil)
		setstr(nil, &c->title, title);
	else
		setstr(nil, &c->curentry->title, title);
}

void
addfile(Cuesheet *c, char *name, int format)
{
	AFile *new;

	lastfile(c);

	new = emalloc(sizeof(*new));
	new->name	= strdup(name);
	new->type	= format;
	new->actual	= actualformat(new);
	new->next	= nil;

	if(c->files == nil)
		c->files = new;
	else
		c->curfile->next = new;

	c->curfile = new;
}

void
addnewtrack(Cuesheet *c, int i)
{
	Entry *new;

	lastentry(c);

	new = emallocz(sizeof(*new), 1);
	new->file = c->curfile;
	new->performer = c->performer;
	new->index = i;

	if(c->entries == nil)
		c->entries = new;
	else
		c->curentry->next = new;

	c->curentry = new;
}

void
settimestamp(Cuesheet *c, int i, Timestamp t)
{
	if(c->curentry == nil)
		parserfatal("timestamp outside of track");

	atleast(c->curentry, i);
	debug("setting timestamp[%d] for %d as %ud frames\n", i, c->curentry->index, t.frames);
	c->curentry->starts[i] = t;
}

char*
extension(char *f)
{
	char *ext = "";

	for(char *c = f; *c != 0; c++)
		if(*c == '.')
			ext = c+1;

	return ext;
}

int
actualformat(AFile *f)
{
	char *ext;

	if(f->type != WAVE)
		return f->type;

	ext = extension(f->name);

	if(strcmp(ext, "wav") == 0)
		return WAVE;
	if(strcmp(ext, "flac") == 0 || strcmp(ext, "fla") == 0)
		return FLAC;

	return UNKNOWN;
}

char*
formatext(int f)
{
	char *tab[] =
	{
		[WAVE]		= "wav",
		[MP3]		= "mp3",
		[AIFF]		= "aiff",
		[BINARY]	= "bin",
		[FLAC]		= "flac",
		[OGG]		= "ogg",
		[OPUS]		= "opus",
		[MOTOROLA]	= ""		/* not sure */
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
