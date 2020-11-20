
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

void parserwarn(char*, ...);
void parserfatal(char*, ...);

/*****/

enum
{
	WAVE, MP3, AIFF, BINARY, MOTOROLA
};

typedef struct
{
	u64int frames;
} Timestamp;

typedef struct AFile
{
	char *name;
	int type;
	struct AFile *next;
} AFile;

typedef struct
{
	u8int maxindex;
	Timestamp *starts;
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

Cuesheet* newsheet(void);

void setperformer(Cuesheet*, char*);
void settitle(Cuesheet*, char*);
void addfile(Cuesheet*, char*, int);
void addnewtrack(Cuesheet*, int);
void settimestamp(Cuesheet*, int, Timestamp);
