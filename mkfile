</$objtype/mkfile

LFILES=\
	cue.l

YFILES=\
	cue.y

HFILES=\
	cuefs.h

OFILES=\
	lex.yy.$O	\
	y.tab.$O	\
	main.$O		\
	misc.$O		\
	cue.$O		\
	fs.$O

LFLAGS=-9

</sys/src/cmd/mkone
