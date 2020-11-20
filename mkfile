</$objtype/mkfile

LFILES=\
	cue.l

YFILES=\
	cue.y

HDR=\
	cuefs.h

OBJ=\
	lex.yy.$O	\
	y.tab.$O	\
	main.$O		\
	misc.$O		\
	cue.$O		\
	fs.$O

CFLAGS=$CFLAGS

%.$O: %.c $HDR
	$CC $CFLAGS $stem.c

$O.out: $OBJ
	$LD $prereq

lex.yy.c: $LFILES y.tab.h
	$LEX -9 $LFILES

y.tab.c y.tab.h: $YFILES mkfile
	$YACC -d $YFILES

clean nuke:V:
	rm -f lex.yy.c y.debug y.tab.[ch] *.[$OS] [$OS].*
