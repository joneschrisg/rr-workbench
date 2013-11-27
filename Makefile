WORKDIR = $(CURDIR)

PYTHON ?= python

RR_DIR ?= $(abspath ../rr)
OBJDIR = $(RR_DIR)/../obj
RR ?= "$(OBJDIR)/bin/rr"

# Current testing build made from git sha1
# c72f46ce99102d6136d5d0e2c1d610d2f3a67692, hg commit ???
FF_SRCDIR = ../mozilla-central
FF_OBJDIR = ../ff-prof
#FF_OBJDIR = ../ff-dbg
XRE_PATH ?= $(FF_OBJDIR)/dist/bin

FF ?= "$(XRE_PATH)/firefox"
XPCSHELL = $(XRE_PATH)/xpcshell

TEST_LOG = $(WORKDIR)/$@.log

DEBUG ?= replay
#RECORD ?= record
RECORD ?= record -b
REPLAY ?= -v replay --autopilot

DBG ?= --debugger=$(RR) --debugger-args='$(RECORD)'

TRACE ?= trace_0

ifdef TEST_PATH
TEST_PATH_ARG = TEST_PATH="$(TEST_PATH)"
endif

PREFS = \
	--setpref=javascript.options.asmjs=false \
	--setpref=javascript.options.baselinejit.chrome=false \
	--setpref=javascript.options.baselinejit.content=false \
	--setpref=javascript.options.ion.chrome=false \
	--setpref=javascript.options.ion.content=false \
	--setpref=javascript.options.ion.parallel_compilation=false

# Shortcut for what you're currently working on, to save typing
default: clean record-bug-845190

#
# XXX this incantation records a trace of kernel scheduling decisions
#
#   sudo make DBG="--debugger=perf --debugger-args='sched record -o /home/cjones/rr/workbench/sched.data /home/cjones/rr/rr/obj/bin/rr record --filter_lib=/home/cjones/rr/rr/obj/lib/librr_syscall_buffer.so'"
#
# Then to see the context switches,
#
#   perf sched map
#

.PHONY: help
help::
	@echo "Available targets are documented below.  Common options:"
	@echo "  make DBG='[--debugger=program [--debugger-args=args]]' ..."
	@echo "    Use a test 'debugger' other than |rr record|, which"
	@echo "    is the default."
	@echo "  --or--"
	@echo "  make RECORD='recording-options' ..."
	@echo "    Adjust the arguments for recording that are passed to rr,"
	@echo "    without having to use the verbose DBG='' syntax above."
	@echo


.PHONY: cycle
cycle: clean record-reftest replay


.PHONY: clean
help::
	@echo "  make clean"
	@echo "    Remove all trace directories."
clean:
	rm -rf trace_* *.o *.so /tmp/rr-test-*


.PHONY: debug
help::
	@echo "  make [TRACE=dir] debug"
	@echo "    Start a debug server for TRACE (default trace_0)."
debug:
	$(RR) $(DEBUG) $(TRACE)


.PHONY: record-firefox
help::
	@echo "  make record-firefox"
	@echo "    Record firefox running standalone."
record-firefox:
	$(RR) $(RECORD) $(FF) -no-remote -P garbage


.PHONY: record-mochitest
help::
	@echo "  make [TEST_PATH=dir] record-mochitest"
	@echo "    Record firefox running the mochitest suite, optionally"
	@echo "    just the TEST_PATH tests."

record-mochitest:
# The mochitest harness helpfully chdir()s to a tmp directory and then
# blows it away when the test suite finishes.  This blows away our
# recorded trace as well.  So explicitly save them to the workbench
# directory.
	rm -f $(TEST_LOG)
	_RR_TRACE_DIR="$(WORKDIR)" make -C $(FF_OBJDIR) \
		EXTRA_TEST_ARGS="$(DBG) $(PREFS)" \
		$(TEST_PATH_ARG) \
		mochitest-plain


.PHONY: record-mochitest-chrome
help::
	@echo "  make [TEST_PATH=dir] record-mochitest-chrome"
	@echo "    Record firefox running the mochitest-chrome suite,"
	@echo "    optionally just the TEST_PATH tests."

record-mochitest-chrome:
# The mochitest harness helpfully chdir()s to a tmp directory and then
# blows it away when the test suite finishes.  This blows away our
# recorded trace as well.  So explicitly save them to the workbench
# directory.
	rm -f $(TEST_LOG)
	_RR_TRACE_DIR="$(WORKDIR)" make -C $(FF_OBJDIR) \
		EXTRA_TEST_ARGS="$(DBG) $(PREFS)" \
		$(TEST_PATH_ARG) \
		mochitest-chrome


.PHONY: record-crashtest
help::
	@echo "  make [TEST_PATH=dir] record-crashtest"
	@echo "    Record firefox running the crashtest suite, optionally"
	@echo "    just the TEST_PATH tests."

record-crashtest:
	rm -f $(TEST_LOG)
	TEST_PATH=$(TEST_PATH) \
	_RR_TRACE_DIR="$(WORKDIR)" make -C $(FF_OBJDIR) \
		EXTRA_TEST_ARGS="$(DBG) $(PREFS)" \
		crashtest

#	$(RR) $(RECORD) $(FF) -no-remote -P garbage -reftest file:///home/cjones/rr/mozilla-central/$(TEST_PATH)

.PHONY: record-reftest
help::
	@echo "  make [TEST_PATH=dir] record-reftest"
	@echo "    Record firefox running the reftest suite, optionally"
	@echo "    just the TEST_PATH tests."

record-reftest:
#	_RR_TRACE_DIR="$(WORKDIR)" make -C $(FF_OBJDIR) \
		EXTRA_TEST_ARGS="--log-file=$(TEST_LOG) $(DBG)" \
		$(TEST_PATH_ARG) \
		reftest
#	$(RR) $(RECORD) $(FF) -no-remote -P garbage -reftest file://$(abspath $(FF_SRCDIR)/$(TEST_PATH)/reftest.list)
#	$(RR) $(RECORD) $(FF) -no-remote -P garbage -reftest file:///home/cjones/rr/mozilla-central/layout/reftests/transform/reftest.list
	$(RR) $(RECORD) -c2500 $(FF) -no-remote -P garbage -reftest file:///home/cjones/rr/mozilla-central/layout/reftests/transform/reftest.list
#	$(RR) $(RECORD) $(FF) -no-remote -P garbage -reftest file:///home/cjones/rr/mozilla-central/netwerk/test/reftest/reftest.list
#	$(RR) $(RECORD) -c2500 $(FF) -no-remote -P garbage -reftest file:///home/cjones/rr/mozilla-central/netwerk/test/reftest/reftest.list
#	$(RR) $(RECORD) -c250 $(FF) -no-remote -P garbage -reftest file:///home/cjones/rr/mozilla-central/netwerk/test/reftest/reftest.list


.PHONY: record-bug-845190
help::
	@echo "  make record-bug-845190"
	@echo "    Run the xpcshell test that's triggering this top orange."
record-bug-845190:
	_RR_TRACE_DIR="$(WORKDIR)" python $(XPCSHELL_DIR)/runxpcshelltests.py \
		$(DBG) \
		--test-path=test_645970.js \
		--xre-path=$(XRE_PATH) \
		--verbose \
		$(XPCSHELL) \
		$(XPCSHELL_DIR)/tests/toolkit/components/search/tests/xpcshell


.PHONY: replay
help::
	@echo "  make [TRACE=dir] replay"
	@echo "    Replay the recording TRACE (default trace_0) on autopilot."
replay:
	$(RR) $(REPLAY) $(TRACE)


.PHONY: syscall-histogram
help::
	@echo " make [TRACES=trace_dir...] syscall-histogram"
	@echo "    Build a histogram of unfiltered syscalls in local traces."
	@echo "    By default, all traces are counted.  Specify TRACES to"
	@echo "    accumulate a different set."
TRACES ?= $(shell find . -maxdepth 1 -name 'trace_*')
syscall-histogram:
	./syscall-histogram $(TRACES)


CFLAGS = -Wall -Werror -g -O0 -m32 -pthread

bad_syscall: bad_syscall.c

launch: launch.c

lcmp: lcmp.c

libseccomptrace.so: Makefile seccomptrace.c
	$(CC) $(CFLAGS) -fPIC -shared -o $@ seccomptrace.c

regtrace: libseccomptrace.so regtrace.c

status2text: status2text.c

# XXX add me to rr tree?
librrmon.so: rrmon.o
	$(CC) $(CFLAGS) -shared -o $@ $<

LLVM_PLUGIN_RELDIR = lib/Transforms/PathLogging
LLVM_SRCDIR = $(HOME)/src/llvm-clang/llvm
LLVM_OBJDIR = $(HOME)/src/llvm-clang/build

paths: Makefile PathLogging_plugin libpathlogging.so paths.c
	clang -O1 -Xclang -load -Xclang $(WORKDIR)/PathLogging.so \
		-pthread paths.c -o paths

#		-Wl,-rpath -Wl,$(WORKDIR) -L $(WORKDIR) -lpathlogging

.PHONY: PathLogging_plugin
PathLogging_plugin:
	touch $(LLVM_SRCDIR)/$(LLVM_PLUGIN_RELDIR)/PathLogging.cpp
	make -C $(LLVM_OBJDIR)/$(LLVM_PLUGIN_RELDIR)
	ln -fs $(LLVM_OBJDIR)/Release+Asserts/lib/PathLogging.so PathLogging.so

# XXX add me to rr tree
libpathlogging.so: Makefile path_logging.c
	$(CC) $(CFLAGS) -I $(RR_DIR)/include -pthread -fPIC -shared \
		-o $@ path_logging.c

##-----------------------------------------------------------------------------
## Temporariily obsolete code for running FF from nightly builds.
## This is no longer possible because Gecko has evolved beyond rr's
## capabilities.
##
# FF_DIR ?= .ff
# # XXX: sigh, no debug nightly builds.  Nor symbolic links to "latest"
# # or something.  So this is an arbitrarily-chosen, healthy-looking
# # build from 2013/06/20.
# FF_URL ?= http://ftp.mozilla.org/pub/mozilla.org/firefox/tinderbox-builds/mozilla-central-linux-debug/1371733138/
# #FF_URL ?= http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-trunk/

# .PHONY: update-firefox
# help::
# 	@echo "  make [FF_URL=url] update-firefox"
# 	@echo "    Blow away the current firefox build and testsuite and"
# 	@echo "    download the latest from FF_URL (defaulting to nightly"
# 	@echo "    builds of trunk)."
# update-firefox:
# 	rm -rf $(FF_DIR)
# 	mkdir $(FF_DIR)
# 	cd $(FF_DIR) && \
# 		wget -nd -r -l 1 \
# 			-A 'firefox*linux-i686.tar.bz2,firefox*linux-i686*.tests.zip' \
# 			$(FF_URL) && \
# 		tar jxf firefox-*.tar.bz2 && \
# 		unzip -q firefox-*.zip
