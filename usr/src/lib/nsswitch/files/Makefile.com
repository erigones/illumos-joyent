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
#
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright 2012 Nexenta Systems, Inc.  All rights reserved

LIBRARY =	libnss_files.a
VERS =		.1

OBJECTS =	bootparams_getbyname.o	\
		ether_addr.o		\
		files_common.o		\
		getgrent.o		\
		gethostent.o		\
		gethostent6.o		\
		getnetent.o		\
		getnetgrent.o		\
		getprojent.o		\
		getprotoent.o		\
		getpwnam.o		\
		getrpcent.o		\
		getprinter.o		\
		getservent.o		\
		getspent.o		\
		getauthattr.o		\
		getprofattr.o		\
		getexecattr.o		\
		getuserattr.o		\
		getauuser.o		\
		netmasks.o		\
		tsol_getrhent.o		\
		tsol_gettpent.o

# include common nsswitch library definitions.
include		../../Makefile.com

# install this library in the root filesystem
include ../../../Makefile.rootfs

CPPFLAGS +=	-I../../../common/inc

LDLIBS +=	-lnsl
DYNLIB1 =	nss_files.so$(VERS)

COMPATLINKS=	usr/lib/$(DYNLIB1) \
		etc/lib/$(DYNLIB1)
COMPATLINKS64=	usr/lib/$(MACH64)/$(DYNLIB1)

$(ROOT)/usr/lib/$(DYNLIB1) := COMPATLINKTARGET=../../lib/$(DYNLIB1)
$(ROOT)/usr/lib/$(MACH64)/$(DYNLIB1):= \
	COMPATLINKTARGET=../../../lib/$(MACH64)/$(DYNLIB1)
$(ROOT)/usr/lib/$(DYNLIB1) := COMPATLINKTARGET=../../lib/$(DYNLIB1)
$(ROOT)/usr/lib/$(MACH64)/$(DYNLIB1):= \
	COMPATLINKTARGET=../../../lib/$(MACH64)/$(DYNLIB1)

$(ROOT)/etc/lib/$(DYNLIB1) := COMPATLINKTARGET= ../../lib/$(DYNLIB1)

all: $(DYNLIB1)

pics/getnetgrent.o := CERRWARN=
pics/getnetgrent.o := SMATCH=
