# Copyright 2009-2014
# Kaz Kylheku <kaz@kylheku.com>
# Vancouver, Canada
# All rights reserved.
#
# Redistribution of this software in source and binary forms, with or without
# modification, is permitted provided that the following two conditions are met.
#
# Use of this software in any manner constitutes agreement with the disclaimer
# which follows the two conditions.
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
#
# THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL THE
# COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DAMAGES, HOWEVER CAUSED,
# AND UNDER ANY THEORY OF LIABILITY, ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

-include config/config.make

VERBOSE :=
CFLAGS := -iquote $(conf_dir) -iquote $(top_srcdir) \
          $(LANG_FLAGS) $(DIAG_FLAGS) \
          $(DBG_FLAGS) $(PLATFORM_FLAGS) $(EXTRA_FLAGS)
CFLAGS += -iquote mpi-$(mpi_version)
CFLAGS := $(filter-out $(REMOVE_FLAGS),$(CFLAGS))

ifneq ($(subst g++,@,$(notdir $(CC))),$(notdir $(CC)))
CFLAGS := $(filter-out -Wmissing-prototypes -Wstrict-prototypes,$(CFLAGS))
endif

# TXR objects
ADD_CONF = $(addprefix $(1)/,$(2))
EACH_CONF = $(foreach conf,opt dbg,$(call ADD_CONF,$(conf),$(1)))

OBJS-y := # make sure OBJ-y is a value variable, not a macro variable
OBJS := txr.o lex.yy.o y.tab.o match.o lib.o regex.o gc.o unwind.o stream.o
OBJS += arith.o hash.o utf8.o filter.o eval.o rand.o combi.o sysif.o
OBJS-$(debug_support) += debug.o
OBJS-$(have_syslog) += syslog.o
OBJS-$(have_posix_sigs) += signal.o

ifneq ($(have_git),)
SRCS := $(addprefix $(top_srcdir)/,\
                    $(filter-out lex.yy.c y.tab.c y.tab.h,\
                                 $(shell git --work-tree=$(top_srcdir) \
				             --git-dir=$(top_srcdir)/.git \
					      ls-files "*.c" "*.h" "*.l" "*.y")))
endif

# MPI objects
MPI_OBJ_BASE=mpi.o mplogic.o

MPI_OBJS := $(addprefix mpi-$(mpi_version)/,$(MPI_OBJ_BASE))

OBJS += $(MPI_OBJS)

DBG_OBJS := $(call ADD_CONF,dbg,$(OBJS) $(OBJS-y))
OPT_OBJS := $(call ADD_CONF,opt,$(OBJS) $(OBJS-y))
OBJS := $(DBG_OBJS) $(OPT_OBJS)

TXR := ./$(PROG)

.SUFFIXES:
MAKEFLAGS += --no-builtin-rules

V = $(if $(VERBOSE),,@)

# Filtering out $(DEP_$@) allows the abbreviated output to show just the direct
# prerequisites without the long laundry list of additional dependencies.
ABBREV = $(if $(VERBOSE),\
           @:,\
	    @printf "%s %s -> %s\n" $(1) "$(filter-out $(DEP_$@),$^)" $@)
ABBREV3 = $(if $(VERBOSE),@:,@printf "%s %s -> %s\n" $(1) "$(3)" $(2))

define DEPGEN
$(V)sed -e '1s/^/DEP_/' -e '1s/: [^ ]\+/ :=/' < $(1) > $(1:.d=.v)
endef

dbg/%.o: %.c
	$(call ABBREV,CC)
	$(V)mkdir -p $(dir $@)
	$(V)$(CC) -MMD -MT $@ $(CFLAGS) -c -o $@ $<
	$(call DEPGEN,${@:.o=.d})

opt/%.o: %.c
	$(call ABBREV,CC)
	$(V)mkdir -p $(dir $@)
	$(V)$(CC) -MMD -MT $@ $(OPT_FLAGS) $(CFLAGS) -c -o $@ $<
	$(call DEPGEN,${@:.o=.d})

# The following pattern rule is used for test targets built by configure
%.o: %.c
	$(call ABBREV,CC,$<)
	$(V)$(CC) $(OPT_FLAGS) $(CFLAGS) -c -o $@ $<

ifeq ($(PROG),)
.PHONY: notconfigured
notconfigured:
	$(V)echo "configuration missing: you didn't run the configure script!"
	$(V)exit 1
endif

.PHONY: all
all: $(BUILD_TARGETS)

$(PROG): $(OPT_OBJS)
	$(call ABBREV,LINK)
	$(V)$(CC) $(OPT_FLAGS) $(CFLAGS) -o $@ $^ -lm

$(PROG)-dbg: $(DBG_OBJS)
	$(call ABBREV,LINK)
	$(V)$(CC) $(CFLAGS) -o $@ $^ -lm

VPATH := $(top_srcdir)

# Newline constant
define NL


endef

define DEP
$(1): $(2)

$(eval $(foreach item,$(1),DEP_$(item) += $(2)$(NL)))
endef

# Pull in dependencies
-include $(OBJS:.o=.d) $(OBJS:.o=.v)

# Add dependencies
$(call DEP,$(OBJS),$(conf_dir)/config.make $(conf_dir)/config.h)
$(call DEP,opt/lex.yy.o dbg/lex.yy.o,y.tab.h)


$(eval $(foreach dep,$(OPT_OBJS:.o=.d) $(DBG_OBJS:.o=.d),\
          $(call DEP_INSTANTIATE,$(dep))))

lex.yy.c: parser.l
	$(call ABBREV,LEX)
	$(V)rm -f $@
	$(V)$(LEX) $(LEX_DBG_FLAGS) $<
	$(V)chmod a-w $@

y.tab.h: y.tab.c
	$(V)if ! [ -e y.tab.h ] ; then                            \
	  echo "Someone removed y.tab.h but left y.tab.c" ;       \
	  echo "Remove y.tab.c and re-run make" ;                 \
	  exit 1;                                                 \
	fi

y.tab.c: parser.y
	$(call ABBREV,YACC)
	$(V)rm -f y.tab.c
	$(V)if $(YACC) -v -d $< ; then chmod a-w y.tab.c ; true ; else rm y.tab.c ; false ; fi

# Suppress useless sccs id array and unused label warning in byacc otuput.
# Bison-generated parser also tests for this lint define.
$(call EACH_CONF,y.tab.o): CFLAGS += -Dlint

# txr.c needs to know the relative datadir path to do some sysroot
# calculations.

opt/txr.o: CFLAGS += -DPROG_NAME=\"$(PROG)\" -DTXR_REL_PATH=\"$(bindir_rel)/$(PROG)$(EXE)\"
dbg/txr.o: CFLAGS += -DPROG_NAME=\"$(PROG)-dbg\" -DTXR_REL_PATH=\"$(bindir_rel)/$(PROG)-dbg$(EXE)\"
$(call EACH_CONF,txr.o): CFLAGS += -DEXE_SUFF=\"$(EXE)\"
$(call EACH_CONF,txr.o): CFLAGS += -DTXR_VER=\"$(txr_ver)\"

$(call EACH_CONF,$(MPI_OBJS)): CFLAGS += -DXMALLOC=chk_malloc -DXREALLOC=chk_realloc
$(call EACH_CONF,$$(MPI_OBJS)): CFLAGS += -DXCALLOC=chk_calloc -DXFREE=free

.PHONY: rebuild
rebuild: clean repatch $(PROG)

.PHONY: clean
clean: conftest.clean tests.clean
	rm -f $(PROG)$(EXE) $(PROG)-dbg$(EXE) y.tab.c lex.yy.c y.tab.h y.output
	rm -rf opt dbg

.PHONY: repatch
repatch:
	cd mpi-$(mpi_version); quilt pop -af
	cd mpi-$(mpi_version); quilt push -a

.PHONY: distclean
distclean: clean
	rm -rf $(conf_dir)
	rm -rf mpi-$(mpi_version)

TESTS_TMP := txr.test.out
TESTS_OUT := $(addprefix tst/,\
		  $(patsubst %.txr,%.out,\
		             $(shell find -H tests -name '*.txr' | sort)))
TESTS_OK := $(TESTS_OUT:.out=.ok)

.PHONY: tests
tests: $(TESTS_OK)
	$(V)echo "** tests passed!"

tst/tests/001/%: TXR_ARGS := tests/001/data
tst/tests/001/query-1.out: TXR_OPTS := -B
tst/tests/001/query-2.out: TXR_OPTS := -B
tst/tests/001/query-4.out: TXR_OPTS := -B
tst/tests/002/%: TXR_OPTS := -DTESTDIR=tests/002
tst/tests/004/%: TXR_ARGS := -a 123 -b -c
tst/tests/005/%: TXR_ARGS := tests/005/data
tst/tests/005/%: TXR_OPTS := -B
tst/tests/006/%: TXR_ARGS := tests/006/data
tst/tests/006/%: TXR_OPTS := -B
tst/tests/006/freeform-3.out: TXR_ARGS := tests/006/passwd
tst/tests/008/tokenize.out: TXR_ARGS := tests/008/data
tst/tests/008/configfile.out: TXR_ARGS := tests/008/configfile
tst/tests/008/students.out: TXR_ARGS := tests/008/students.xml
tst/tests/008/soundex.out: TXR_ARGS := soundex sowndex lloyd lee jackson robert
tst/tests/008/filtenv.out: TXR_OPTS := -B
tst/tests/009/json.out: TXR_ARGS := $(addprefix tests/009/,webapp.json pass1.json)
tst/tests/010/align-columns.out: TXR_ARGS := tests/010/align-columns.dat
tst/tests/010/block.out: TXR_OPTS := -B
tst/tests/010/reghash.out: TXR_OPTS := -B

tst/tests/002/%: TXR_SCRIPT_ON_CMDLINE := y

tst/tests/011/%: TXR_DBG_OPTS :=

.PRECIOUS: tst/%.out
tst/%.out: %.txr
	$(call ABBREV,TXR)
	$(V)mkdir -p $(dir $@)
	$(V)$(if $(TXR_SCRIPT_ON_CMDLINE),\
	       $(TXR) $(TXR_DBG_OPTS) $(TXR_OPTS) -c "$$(cat $<)" \
	          $(TXR_ARGS) > $(TESTS_TMP),\
	       $(TXR) $(TXR_DBG_OPTS) $(TXR_OPTS) $< $(TXR_ARGS) > $(TESTS_TMP))
	$(V)mv $(TESTS_TMP) $@

%.ok: %.out
	$(V)diff -u $(patsubst tst/%.out,%.expected,$<) $<
	$(V)touch $@

%.expected: %.out
	cp $< $@

.PHONY: tests.clean
tests.clean:
	rm -rf tst

define GREP_CHECK
	$(V)if [ $$(grep -E $(1) $(SRCS) | wc -l) -ne $(3) ] ; then \
	      echo "New '$(2)' occurrences have been found:" ; \
	      grep -n -E $(1) $(SRCS) \
	        | sed -e 's/\(.*:.*:\).*/\1 $(2)/' \
	        | grep $(4) ; \
	      exit 1 ; \
	    fi
endef

.PHONY: enforce
enforce:
ifneq ($(have_git),)
	$(call GREP_CHECK,'\<void[	 ]*\*',void *,1,-v 'typedef void \*yyscan_t')
	$(call GREP_CHECK,'	',tabs,0,'.')
	$(call GREP_CHECK,' $$',trailing spaces,0,'.')
else
	echo "enforce requires git"
	exit 1
endif

#
# Installation macro.
#
# $1 - chmod perms
# $2 - source file(s)
# $3 - dest directory
#
define INSTALL
	$(call ABBREV3,INSTALL,$(3),$(2))
	$(V)mkdir -p $(3)
	$(V)cp -f $(2) $(3)
	$(V)chmod $(1) $(3)/$(notdir $(2))
	$(V)for x in $(2) ; do touch -r $$x $(3)/$$(basename $$x) ; done
endef

PREINSTALL := :

.PHONY: install
install: $(PROG)
	$(V)$(PREINSTALL)
	$(call INSTALL,0755,txr$(EXE),$(DESTDIR)$(bindir))
	$(call INSTALL,0444,$(top_srcdir)/LICENSE,$(DESTDIR)$(datadir))
	$(call INSTALL,0444,$(top_srcdir)/METALICENSE,$(DESTDIR)$(datadir))
	$(call INSTALL,0444,$(top_srcdir)/txr.1,$(DESTDIR)$(mandir)/man1)
	$(call INSTALL,0444,$(top_srcdir)/share/txr/stdlib/*.txr,$(DESTDIR)$(datadir)/stdlib)

.PHONY: unixtar gnutar zip

unixtar gnutar zip: DESTDIR=pkg
zip: prefix=/txr
unixtar gnutar zip: PREINSTALL=rm -rf pkg

unixtar: install
	cd pkg; pwd; find . | pax -M uidgid -w -f ../txr-$(txr_ver)-bin.tar -x ustar ; ls ../*.tar
	pwd
	compress txr-$(txr_ver)-bin.tar

gnutar: install
	tar --owner=0 --group=0 -C pkg -czf txr-$(txr_ver)-bin.tar.gz .

zip: install
	cd pkg; zip -r ../txr-$(txr_ver)-bin.zip .

#
# Install the tests as well as the script to run them
#
install-tests:
	$(V)rm -rf tst
	$(V)mkdir -p $(DESTDIR)$(datadir)
	$(call ABBREV3,INSTALL,$(DESTDIR)$(datadir),tests)
	$(V)(find tests | cpio -o 2> /dev/null) \
	    | (cd $(DESTDIR)$(datadir) ; cpio -idum 2> /dev/null)
	$(V)(echo "#!/bin/sh" ; \
	     echo "set -ex" ; \
	     echo "cd $(datadir)" ; \
	     make -s -n tests VERBOSE=y TXR=$(bindir)/txr) \
	     > run.sh
	$(call INSTALL,0755,run.sh,$(DESTDIR)$(datadir)/tests)

#
# Generate web page from man page
#
txr-manpage.html: txr.1 genman.txr
	man2html $< | $(TXR) genman.txr - > $@

txr-manpage.pdf: txr.1
	tbl $< | pdfroff -man --no-toc - > $@

#
# Special targets used by ./configure
#

conftest: conftest.c
	$(CC) $(CFLAGS) -o $@ $^ -lm

conftest2: conftest1.c conftest2.c
	$(CC) $(CFLAGS) -o $@ $^ -lm

conftest.syms: conftest.o
	$(NM) -n -t o -P $^ > $@

.PHONY: conftest.yacc
conftest.yacc:
	$(V)echo $(YACC)

.PHONY: conftest.ccver
conftest.ccver:
	$(V)$(CC) --version

.PHONY: conftest.clean
conftest.clean:
	$(V)rm -f conftest$(EXE) conftest.[co] \
	conftest2$(EXE) conftest[12].[oc] \
	conftest.err conftest.syms
