</$objtype/mkfile

TARG=cuefs
BIN=/$objtype/bin
RCBIN=/rc/bin

RC=\
	cuesplit

MAN=\
	4/cuefs	\
	1/cuesplit

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

LFLAGS=-9n
YFLAGS=$YFLAGS

install:V: ${RC:%=$RCBIN/%} ${MAN:%=/sys/man/%}

# TODO better automate manpage installation, etc

/sys/man/4/cuefs: cuefs.4.man
	cp $prereq $target

/sys/man/1/cuesplit: cuesplit.1.man
	cp $prereq $target

$RCBIN/%: %
	cp $prereq $target

acid:V: debug.$objtype.acid
clean nuke:V: cleanacid

cleanacid:V:
	rm -f debug.^($CPUS)^.acid

debug.$objtype.acid: $HFILES
	$CC -a *.c >$target
