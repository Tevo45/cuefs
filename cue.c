#include <u.h>
#include <libc.h>

#include "cuefs.h"
#include "y.tab.h"

Timestamp
parsetime(int min, int sec, int frames)
{
	sec += min*60;
	frames += sec*75;

	return (Timestamp){frames};
}

Cuesheet*
newsheet(void)
{
	return emallocz(sizeof(Cuesheet), 1);
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
	new->name = strdup(name);
	new->type = format;
	new->next = nil;

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
	c->curentry->starts[i] = t;
}
