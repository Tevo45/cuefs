</$objtype/mkfile

LFILES=\
	cue.l

YFILES=\
	cue.y

HFILES=\
	cuefs.h		\
	y.tab.h

OFILES=\
	lex.yy.$O	\
	y.tab.$O	\
	main.$O		\
	misc.$O		\
	cue.$O		\
	fs.$O

</sys/src/cmd/mkone

LFLAGS=-9
