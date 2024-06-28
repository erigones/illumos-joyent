#
# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms of version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source.  A copy of the CDDL is also available via the Internet at
# http://www.illumos.org/license/CDDL.
#

#
# Copyright 2020 Toomas Soome <tsoome@me.com>
#

LIBRARY=	libzfsbootenv.a
VERS=		.1

OBJECTS=	\
		lzbe_device.o \
		lzbe_pair.o \
		lzbe_util.o

include ../../Makefile.lib

LIBS=		$(DYNLIB)

SRCDIR=		../common

CSTD=		$(CSTD_GNU99)

LDLIBS +=	-lzfs -lnvpair -lc
CPPFLAGS +=	-I$(SRC)/uts/common/fs/zfs


CLOBBERFILES += $(LIBRARY)

.KEEP_STATE:

all: $(LIBS)

include ../../Makefile.targ
