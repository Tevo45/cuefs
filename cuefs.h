
extern char *fname;
extern int infd, outfd;

/* lex/yacc */
int yylex(void);
int yyparse(void);
void yyerror(char*);

/* misc.c */
void* erealloc(void*, ulong);
void* emallocz(ulong, int);
void* emalloc(ulong);
char* esmprint(char*, ...);

char* setstr(char*, char**, char*);
char* strreplace(char*, char, char);

#pragma varargk argpos parserwarn 1
#pragma varargk argpos parserfatal 1
#pragma varargk argpos debug 1

void parserwarn(char*, ...);
void parserfatal(char*, ...);

void debug(char*, ...);

extern int verbosity;

/*****/

enum
{
	WAVE, MP3, AIFF, BINARY /* PCM LE */, MOTOROLA /* PCM BE */,
	/**/
	AAC, FLAC, OGG, OPUS, UNKNOWN
};

enum
{
	FLAG_DCP	= (1<<0),
	FLAG_4CH	= (1<<1),
	FLAG_PRE	= (1<<2),
	FLAG_SCMS	= (1<<3),
};

typedef struct Timestamp Timestamp;
typedef struct AFile AFile;
typedef struct Start Start;
typedef struct Entry Entry;
typedef struct Cuesheet Cuesheet;

struct Timestamp
{
	u64int frames;
};

struct AFile
{
	int type, actual;
	AFile *next;
	char *name;
};

struct Start
{
	Timestamp;
	u8int index;
	Start *next;
};

struct Entry
{
	Cuesheet *sheet;
	Start *starts;
	AFile *file;
	int index, flags;
	char *title, *performer, *isrc;
	Entry *next;
};

struct Cuesheet
{
	char *title, *performer, *mcn;
	AFile *files, *curfile;
	Entry *entries, *curentry;
};

extern Cuesheet *cursheet;

Timestamp parsetime(int, int, int);
double t2sec(Timestamp);
double of2sec(uint, uint, uint, vlong);

Cuesheet* newsheet(void);
void freesheet(Cuesheet*);

void setisrc(Cuesheet*, char*);
void setflags(Cuesheet*, int);
void setmcn(Cuesheet*, char*);
void setperformer(Cuesheet*, char*);
void settitle(Cuesheet*, char*);
void addfile(Cuesheet*, char*, int);
void addnewtrack(Cuesheet*, int);
void settimestamp(Cuesheet*, int, Timestamp);

char* formatext(int);
char* fileext(AFile*);
int actualformat(AFile*);

int prefoutfmt(int);

static char *Estub = "not yet";
static char *Eunsupported = "unsupported format";

void cuefsinit(Cuesheet*, char*, int, int);
