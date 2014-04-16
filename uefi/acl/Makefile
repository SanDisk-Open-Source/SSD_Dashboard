#
# Copyright (c) 2000-2006 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = .
HAVE_BUILDDEFS = $(shell test -f $(TOPDIR)/include/builddefs && echo yes || echo no)

ifeq ($(HAVE_BUILDDEFS), yes)
include $(TOPDIR)/include/builddefs
endif

CONFIGURE = configure include/builddefs include/config.h
LSRCFILES = configure configure.in aclocal.m4 Makepkgs install-sh exports \
	README VERSION

LDIRT = config.log .dep config.status config.cache confdefs.h conftest* \
	Logs/* built .census install.* install-dev.* install-lib.* *.gz

LIB_SUBDIRS = include libmisc libacl
TOOL_SUBDIRS = getfacl setfacl chacl m4 man doc po test examples build debian

SUBDIRS = $(LIB_SUBDIRS) $(TOOL_SUBDIRS)

default: include/builddefs include/config.h
ifeq ($(HAVE_BUILDDEFS), no)
	$(MAKE) -C . $@
else
	$(MAKE) $(SUBDIRS)
endif

# tool/lib dependencies
libacl: libmisc
getfacl setfacl chacl: libacl

ifeq ($(HAVE_BUILDDEFS), yes)
include $(BUILDRULES)
else
clean:	# if configure hasn't run, nothing to clean
endif

include/builddefs:
	autoconf
	./configure \
		--prefix=/ \
		--exec-prefix=/ \
		--sbindir=/bin \
		--bindir=/usr/bin \
		--libdir=/lib \
		--libexecdir=/usr/lib \
		--enable-lib64=yes \
		--includedir=/usr/include \
		--mandir=/usr/share/man \
		--datadir=/usr/share \
		$$LOCAL_CONFIGURE_OPTIONS
	touch .census

include/config.h: include/builddefs
## Recover from the removal of $@
	@if test -f $@; then :; else \
		rm -f include/builddefs; \
		$(MAKE) $(AM_MAKEFLAGS) include/builddefs; \
	fi

aclocal.m4::
	aclocal --acdir=`pwd`/m4 --output=$@

install: default $(addsuffix -install,$(SUBDIRS))
	$(INSTALL) -m 755 -d $(PKG_DOC_DIR)
	$(INSTALL) -m 644 README $(PKG_DOC_DIR)

install-dev: default $(addsuffix -install-dev,$(SUBDIRS))

install-lib: default $(addsuffix -install-lib,$(SUBDIRS))

%-install:
	$(MAKE) -C $* install

%-install-lib:
	$(MAKE) -C $* install-lib

%-install-dev:
	$(MAKE) -C $* install-dev

realclean distclean: clean
	rm -f $(LDIRT) $(CONFIGURE)
	rm -rf autom4te.cache Logs
