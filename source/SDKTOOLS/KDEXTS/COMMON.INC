!IF 0

Copyright (c) 2000  Microsoft Corporation

Module Name:

    common.inc

Abstract:

    This is an include file for makefile.inc in each of the
    directories which has c files that are wrappers for the
    files in this directory.  The WRAPPEDCOBJECTS macro contains
    a list of the objects which are dependent on the files
    in this directory.

    SRCDIR must be defined in each makefile.inc, and will
    always point to the directory that this file is in.


Author:

    Wesley Witt (wesw) 10-Aug-1993

!ENDIF


WRAPPEDCOBJECTS =      \
    obj\*\calldata.obj \
    obj\*\crt.obj      \
    obj\*\device.obj   \
    obj\*\driver.obj   \
    obj\*\errorlog.obj \
    obj\*\exceptn.obj  \
    obj\*\filecach.obj \
    obj\*\handle.obj   \
    obj\*\heap.obj     \
    obj\*\help.obj     \
    obj\*\igrep.obj    \
    obj\*\iospace.obj  \
    obj\*\locks.obj    \
    obj\*\lpc.obj      \
    obj\*\memory.obj   \
    obj\*\object.obj   \
    obj\*\physical.obj \
    obj\*\pool.obj     \
    obj\*\process.obj  \
    obj\*\pte.obj      \
    obj\*\ready.obj    \
    obj\*\registry.obj \
    obj\*\srb.obj      \
    obj\*\time.obj     \
    obj\*\token.obj    \
    obj\*\util.obj     \
    obj\*\vad.obj      \
    obj\*\wsle.obj     \
    obj\*\kdexts.obj

$(WRAPPEDCOBJECTS:*=i386):  $(SRCDIR)\$$(@B).c
$(WRAPPEDCOBJECTS:*=mips):  $(SRCDIR)\$$(@B).c
$(WRAPPEDCOBJECTS:*=alpha): $(SRCDIR)\$$(@B).c

kdexts.c: $(BASEDIR)\neptune\sdk\inc\ntverp.h
