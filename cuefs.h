
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

void parserwarn(char*, ...);
void parserfatal(char*, ...);

/*****/

enum
{
	WAVE, MP3, AIFF, BINARY, MOTOROLA,
	/**/
	FLAC, UNKNOWN
};

typedef struct
{
	u64int frames;
} Timestamp;

typedef struct AFile
{
	int type, actual, fd;
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

Cuesheet* newsheet(void);
void freesheet(Cuesheet*);

void setperformer(Cuesheet*, char*);
void settitle(Cuesheet*, char*);
void addfile(Cuesheet*, char*, int);
void addnewtrack(Cuesheet*, int);
void settimestamp(Cuesheet*, int, Timestamp);

char* formatext(AFile*);
int actualformat(AFile*);

static char *Estub = "not yet";
static char *Eunsupported = "unsupported format";

void cuefsinit(Cuesheet*, char*);
