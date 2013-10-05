WORKDIR = $(CURDIR)

RR_DIR ?= $(abspath ../rr)
OBJDIR = $(RR_DIR)/../obj
RR ?= "$(OBJDIR)/bin/rr"

# Current testing build made from git sha1
# 6462f0bb0d18a650e217aae1dba1f7549236a712, hg commit ???
FF_DIR ?= .ff
# XXX: sigh, no debug nightly builds.  Nor symbolic links to "latest"
# or something.  So this is an arbitrarily-chosen, healthy-looking
# build from 2013/06/20.
FF_URL ?= http://ftp.mozilla.org/pub/mozilla.org/firefox/tinderbox-builds/mozilla-central-linux-debug/1371733138/
#FF_URL ?= http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-trunk/
LOG = $(FF_DIR)/mochitest.log
MOCHITEST_DIR = $(FF_DIR)/mochitest
XPCSHELL_DIR = $(FF_DIR)/xpcshell

#XRE_PATH ?= $(FF_DIR)/firefox
XRE_PATH ?= ../ff-prof/dist/bin
#XRE_PATH ?= ../ff-dbg/dist/bin

FF ?= "$(XRE_PATH)/firefox"
#XPCSHELL ?= "$(FF_DIR)/bin/xpcshell"
XPCSHELL = $(XRE_PATH)/xpcshell

DEBUG ?= replay
#RECORD ?= record
RECORD ?= -v record -b
REPLAY ?= -v replay --autopilot

DBG ?= --debugger=$(RR) --debugger-args="$(RECORD)"

TRACE ?= trace_0

ifdef TEST_PATH
TEST_PATH_ARG := --test-path="$(TEST_PATH)"
endif


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
	$(RR) $(RECORD) $(FF) -no-remote -P garbage -reftest file:///home/cjones/rr/mozilla-central/netwerk/test/reftest/reftest.list


.PHONY: record-mochitests
help::
	@echo "  make [TEST_PATH=dir] record-mochitests"
	@echo "    Record firefox running the mochitest suite, optionally"
	@echo "    just the TEST_PATH tests."
record-mochitests:
# The mochitest harness helpfully chdir()s to a tmp directory and then
# blows it away when the test suite finishes.  This blows away our
# recorded trace as well.  So explicitly save them to the workbench
# directory.
	rm -f $(WORKDIR)/$@.log
	_RR_TRACE_DIR="$(WORKDIR)" python "$(MOCHITEST_DIR)/runtests.py" \
		$(DBG) \
		--appname=$(FF) \
		--utility-path="$(FF_DIR)/bin" \
		--extra-profile-file="$(FF_DIR)/bin/plugins" \
		--certificate-path="$(FF_DIR)/certs" \
		--autorun --close-when-done \
		--console-level=INFO --log-file="$(WORKDIR)/$@.log" \
		$(TEST_PATH_ARG)
	@errors=`grep "TEST-UNEXPECTED-" $(WORKDIR)/$@.log` ;\
	if test "$$errors" ; then \
		echo "$@ failed:"; \
		echo "$$errors"; \
		exit 1; \
	fi


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


.PHONY: update-firefox
help::
	@echo "  make [FF_URL=url] update-firefox"
	@echo "    Blow away the current firefox build and testsuite and"
	@echo "    download the latest from FF_URL (defaulting to nightly"
	@echo "    builds of trunk)."
update-firefox:
	rm -rf $(FF_DIR)
	mkdir $(FF_DIR)
	cd $(FF_DIR) && \
		wget -nd -r -l 1 \
			-A 'firefox*linux-i686.tar.bz2,firefox*linux-i686*.tests.zip' \
			$(FF_URL) && \
		tar jxf firefox-*.tar.bz2 && \
		unzip -q firefox-*.zip

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
