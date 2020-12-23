
extern char *fname;
extern int infd;

/* lex/yacc */
int yylex(void);
int yyparse(void);
void yyerror(char*);

/* misc.c */
void* erealloc(void*, ulong);
void* emallocz(ulong, int);
void* emalloc(ulong);

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
	WAVE, MP3, AIFF, BINARY, MOTOROLA,
	/**/
	FLAC, OGG, OPUS, UNKNOWN
};

typedef struct
{
	u64int frames;
} Timestamp;

typedef struct AFile
{
	int type, actual;
	struct AFile *next;
	char *name;
} AFile;

typedef struct
{
	Timestamp *starts;
	u8int maxindex;
} Timestamps;

typedef struct Entry
{
	Timestamps;
	AFile *file;
	int index;
	char *title, *performer;
	struct Entry *next;
} Entry;

typedef struct
{
	char *title, *performer;
	AFile *files, *curfile;
	Entry *entries, *curentry;
} Cuesheet;

extern Cuesheet *cursheet;

Timestamp parsetime(int, int, int);
double t2sec(Timestamp);
double of2sec(uint, uint, uint, vlong);

Cuesheet* newsheet(void);
void freesheet(Cuesheet*);

void setperformer(Cuesheet*, char*);
void settitle(Cuesheet*, char*);
void addfile(Cuesheet*, char*, int);
void addnewtrack(Cuesheet*, int);
void settimestamp(Cuesheet*, int, Timestamp);

char* formatext(int);
char* fileext(AFile*);
int actualformat(AFile*);

static char *Estub = "not yet";
static char *Eunsupported = "unsupported format";

void cuefsinit(Cuesheet*, char*);
