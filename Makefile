WORKDIR = $(CURDIR)

PYTHON ?= python

RR_DIR ?= $(abspath ../rr)
OBJDIR = $(RR_DIR)/../obj
RR ?= "$(OBJDIR)/bin/rr"

SUNSPIDER ?= $(HOME)/src/SunSpider

# Current testing build made from gecko-dev git sha1
# e7567904cc0f161f1042d22333d690fd1f346896, hg commit ???
FF_SRCDIR = $(abspath ../mozilla-central)
#FF_OBJDIR = $(abspath ../ff-dbg)
#FF_OBJDIR = $(abspath ../ff-prof)
FF_OBJDIR = $(abspath ../ff-gcv)
XRE_PATH ?= $(FF_OBJDIR)/dist/bin

FF ?= "$(XRE_PATH)/firefox"
XPCSHELL = $(XRE_PATH)/xpcshell

TEST_LOG = $(WORKDIR)/$@.log

DEBUG ?= -fm replay
RECORD ?= -fm record -b
REPLAY ?= -fm replay --autopilot

DBG ?= --debugger=$(RR) --debugger-args='$(RECORD)' --slowscript
DBG_RFTST ?= --debugger=$(RR) --debugger-args='$(RECORD)'

TRACE ?=

JS_ARGS =

LIBC_OBJDIR ?= $(HOME)/rpmbuild/BUILD/glibc-2.18/obj
PRELOAD_CUSTOM_LIBC ?= LD_PRELOAD="$(LIBC_OBJDIR)/libc.so:$(LIBC_OBJDIR)/nptl/libpthread.so:$(LD_PRELOAD)"


ifdef TEST_PATH
TEST_PATH_ARG = TEST_PATH="$(TEST_PATH)"
endif


##-----------------------------------------------------------------------------
## Targets to automate common tasks

 PREFS = \
	--setpref=javascript.options.ion.parallel_compilation=false

# PREFS = \
# 	--setpref=javascript.options.asmjs=false \
# 	--setpref=javascript.options.baselinejit.chrome=false \
# 	--setpref=javascript.options.baselinejit.content=false \
# 	--setpref=javascript.options.ion.chrome=false \
# 	--setpref=javascript.options.ion.content=false \
# 	--setpref=javascript.options.ion.parallel_compilation=false

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
	rm -rf ~/.rr *.o /tmp/rr-test-*


.PHONY: debug
help::
	@echo "  make [TRACE=dir] debug"
	@echo "    Start a debug server for TRACE (default trace_0)."
debug:
	$(RR) $(DEBUG) $(TRACE)


.PHONY: dif
help::
	@echo "  make dif"
	@echo "    Show a dif of the rr source dir.."
dif:
	cd $(RR_DIR) && grep -rn '\/\*\* \*\/' src/* || git dif


.PHONY: record-firefox
help::
	@echo "  make [URL=url] record-firefox"
	@echo "    Record firefox running standalone, optionally loading URL."
record-firefox:
	$(RR) $(RECORD) $(FF) -no-remote -P garbage $(URL)


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
	make -C $(FF_OBJDIR) \
	 	EXTRA_TEST_ARGS="$(DBG) $(PREFS)" \
	 	$(TEST_PATH_ARG) \
	 	mochitest-plain


strace-mochitest:
	rm -f $(TEST_LOG)
	make -C $(FF_OBJDIR) \
	 	EXTRA_TEST_ARGS="--debugger=strace --debugger-args=-f" \
	 	$(TEST_PATH_ARG) mochitest-plain



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
	make -C $(FF_OBJDIR) \
		EXTRA_TEST_ARGS="$(DBG) $(PREFS)" \
		$(TEST_PATH_ARG) \
		mochitest-chrome


.PHONY: record-mochitest-a11y
help::
	@echo "  make [TEST_PATH=dir] record-mochitest-a11y"
	@echo "    Record firefox running the mochitest-a11y suite,"
	@echo "    optionally just the TEST_PATH tests."

record-mochitest-a11y:
# The mochitest harness helpfully chdir()s to a tmp directory and then
# blows it away when the test suite finishes.  This blows away our
# recorded trace as well.  So explicitly save them to the workbench
# directory.
	rm -f $(TEST_LOG)
	make -C $(FF_OBJDIR) \
		EXTRA_TEST_ARGS="$(DBG) $(PREFS)" \
		$(TEST_PATH_ARG) \
		mochitest-a11y


.PHONY: record-crashtest
help::
	@echo "  make [TEST_PATH=dir] record-crashtest"
	@echo "    Record firefox running the crashtest suite, optionally"
	@echo "    just the TEST_PATH tests."

record-crashtest:
	make -C $(FF_OBJDIR) \
		EXTRA_TEST_ARGS="--log-file=$(TEST_LOG) $(DBG_RFTST)" \
		$(TEST_PATH_ARG) \
		crashtest


.PHONY: record-crashtest-ipc
help::
	@echo "  make [TEST_PATH=dir] record-crashtest-ipc"
	@echo "    Record firefox running the crashtest-ipc suite, optionally"
	@echo "    just the TEST_PATH tests."

record-crashtest-ipc:
	make -C $(FF_OBJDIR) \
		EXTRA_TEST_ARGS="--log-file=$(TEST_LOG) $(DBG_RFTST)" \
		$(TEST_PATH_ARG) \
		crashtest-ipc


.PHONY: record-reftest
help::
	@echo "  make [TEST_PATH=dir] record-reftest"
	@echo "    Record firefox running the reftest suite, optionally"
	@echo "    just the TEST_PATH tests."

record-reftest:
	make -C $(FF_OBJDIR) \
		EXTRA_TEST_ARGS="--log-file=$(TEST_LOG) $(DBG_RFTST)" \
		$(TEST_PATH_ARG) \
		reftest


.PHONY: record-reftest-ipc
help::
	@echo "  make [TEST_PATH=dir] record-reftest"
	@echo "    Record firefox running the reftest-ipc suite, optionally"
	@echo "    just the TEST_PATH tests."

record-reftest-ipc:
	make -C $(FF_OBJDIR) \
		EXTRA_TEST_ARGS="--log-file=$(TEST_LOG) $(DBG_RFTST)" \
		$(TEST_PATH_ARG) \
		reftest-ipc


.PHONY: record-jstestbrowser
help::
	@echo "make [TEST_PATH=dir] record-jstestbrowser"
	@echo "  Record firefox running the jstestbrowser suite, optionally"
	@echo "  just the TEST_PATH tests."

record-jstestbrowser:
	make -C $(FF_OBJDIR) \
		EXTRA_TEST_ARGS="--log-file=$(TEST_LOG) $(DBG)" \
		$(TEST_PATH_ARG) \
		jstestbrowser


.PHONY: record-sunspider
help::
	@echo "  make [JS_ARGS=args] record-sunspider"
	@echo "    Record firefox jsshell running sunspider, optionally"
	@echo "    passing it JS_ARGS."

record-sunspider:
	cd $(SUNSPIDER) && \
		$(RR) $(RECORD) \
		./sunspider --shell=$(XRE_PATH)/js \
			--args="$(JS_ARGS)" \
			--run=30 --suite=sunspider-0.9.1

.PHONY: sunspider
help::
	@echo "  make [JS_ARGS=args] sunspider"
	@echo "    Run sunspider under firefox jsshell optionally"
	@echo "    passing it JS_ARGS."

sunspider:
	cd $(SUNSPIDER) && \
		./sunspider --shell=$(XRE_PATH)/js \
			--args="$(JS_ARGS)" \
			--run=30 --suite=sunspider-0.9.1


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


##-----------------------------------------------------------------------------
## Sundry helper programs

CFLAGS = -Wall -Werror -g -O0 -m32 -pthread

SIMPLE_PROGS = \
	bad_syscall \
	launch \
	lcmp \
	status2text \
	strfutexcmd \
	sysemu \
	vdso_monkeypatch \
	watchpoint

define SIMPLE_PROG_RULE # (1 = progname)
$(1): $(1).c
helper-progs:: Makefile $(1)
endef
$(foreach prog,$(SIMPLE_PROGS),$(eval $(call SIMPLE_PROG_RULE,$(prog))))

libseccomptrace.so: Makefile seccomptrace.c
	$(CC) $(CFLAGS) -fPIC -shared -o $@ seccomptrace.c

regtrace: Makefile libseccomptrace.so regtrace.c

# XXX add me to rr tree?
librrmon.so: rrmon.o
	$(CC) $(CFLAGS) -shared -o $@ $<


##-----------------------------------------------------------------------------
## LLVM plugin to instrument Bell-Larus path tracing

LLVM_PLUGIN_RELDIR = lib/Transforms/PathLogging
LLVM_SRCDIR = $(HOME)/src/llvm-clang/llvm
LLVM_OBJDIR = $(HOME)/src/llvm-clang/build

paths: Makefile PathLogging_plugin libpathlogging.so paths.c
	clang -g -O2 -Xclang -load -Xclang $(WORKDIR)/PathLogging.so \
		-pthread paths.c -o paths \
		-Wl,-rpath -Wl,$(WORKDIR) -L $(WORKDIR) -lpathlogging

.PHONY: PathLogging_plugin
PathLogging_plugin:
	touch $(LLVM_SRCDIR)/$(LLVM_PLUGIN_RELDIR)/PathLogging.cpp
	make -C $(LLVM_OBJDIR)/$(LLVM_PLUGIN_RELDIR)
	ln -fs $(LLVM_OBJDIR)/Release+Asserts/lib/PathLogging.so PathLogging.so

# XXX add me to rr tree
libpathlogging.so: Makefile path_logging.c
	$(CC) -g $(CFLAGS) -I $(RR_DIR)/include -pthread -fPIC -shared \
		-o $@ path_logging.c


mod: Makefile mod.cc
	clang++ -g -O2 -Xclang -load -Xclang $(WORKDIR)/PathLogging.so \
		-pthread -o $@ mod.cc \
		-Wl,-rpath -Wl,$(WORKDIR) -L $(WORKDIR) -lpathlogging -ldl \
		-Wl,--whole-archive $(FF_OBJDIR)/mozglue/build/libmozglue.a \
		-Wl,--no-whole-archive -rdynamic -ldl

libctor.so: Makefile ctor.cc
	clang++ -g -O2 -Xclang -load -Xclang $(WORKDIR)/PathLogging.so \
		-pthread -fPIC -shared -o $@ ctor.cc \
		-Wl,-rpath -Wl,$(WORKDIR) -L $(WORKDIR) -lpathlogging


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
