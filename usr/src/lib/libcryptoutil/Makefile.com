#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#
# Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.
#
# Copyright (c) 2018, Joyent, Inc.

LIBRARY= libcryptoutil.a
VERS=	.1

OBJECTS= \
	debug.o \
	mechstr.o \
	config_parsing.o \
	tohexstr.o \
	mechkeygen.o \
	mechkeytype.o \
	pkcserror.o \
	passutils.o \
	random.o \
	keyfile.o \
	util.o \
	pkcs11_uri.o

include $(SRC)/lib/Makefile.lib
include $(SRC)/lib/Makefile.rootfs

SRCDIR=	../common

LIBS =	$(DYNLIB)
LDLIBS += -lc

CFLAGS +=	$(CCVERBOSE)
CPPFLAGS +=	-D_REENTRANT -D_POSIX_PTHREAD_SEMANTICS -I$(SRCDIR)

CERRWARN +=	-_gcc=-Wno-parentheses
CERRWARN +=	$(CNOWARN_UNINIT)

# not linted
SMATCH=off

all: $(LIBS)


include $(SRC)/lib/Makefile.targ
