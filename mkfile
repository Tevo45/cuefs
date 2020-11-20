</$objtype/mkfile

LFILES=\
	cue.l

YFILES=\
	cue.y

OBJ=\
	lex.yy.$O	\
	y.tab.$O	\
	main.$O		\
	misc.$O		\
	cue.$O		\
	fs.$O

%.$O: %.c
	$CC $CFLAGS $prereq

$O.out: $OBJ
	$LD $prereq

lex.yy.c: $LFILES y.tab.h
	$LEX -9 $LFILES

y.tab.c y.tab.h: $YFILES
	$YACC -d $YFILES

clean nuke:V:
	rm -f lex.yy.c y.debug y.tab.[ch] *.[$OS] [$OS].*
