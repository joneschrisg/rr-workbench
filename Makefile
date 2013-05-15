WORKDIR = $(CURDIR)

RR_DIR ?= ../rr
FF_DIR ?= .ff
# XXX: sigh, no debug nightly builds.  Nor symbolic links to "latest"
# or something.  So this is an arbitrarily-chosen, healthy-looking
# build from 2013/05/14.
FF_URL ?= http://ftp.mozilla.org/pub/mozilla.org/firefox/tinderbox-builds/mozilla-central-linux-debug/1368477222/
#FF_URL ?= http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-trunk/

OBJDIR = $(RR_DIR)/obj
LOG = $(FF_DIR)/mochitest.log
MOCHITEST_DIR = $(FF_DIR)/mochitest
XPCSHELL_DIR = $(FF_DIR)/xpcshell

RR = "$(OBJDIR)/bin/rr"
LIB = "$(OBJDIR)/lib/librr_wrap_syscalls.so"

FF = "$(FF_DIR)/firefox/firefox"
XPCSHELL = "$(FF_DIR)/bin/xpcshell"

RECORD = --record --filter_lib=$(LIB)

DBG ?= --debugger=$(RR) --debugger-args="$(RECORD)"

ifdef TEST_PATH
TEST_PATH_ARG := --test-path="$(TEST_PATH)"
endif

.PHONY: help
help::
	@echo "Available targets are documented below.  Common options:"
	@echo "  make DBG='[--debugger=program [--debugger-args=args]]' ..."
	@echo "    Use a test 'debugger' other than |rr --record|, which"
	@echo "    is the default."
	@echo


.PHONY: clean
help::
	@echo "  make clean"
	@echo "    Remove all trace directories."
clean:
	rm -rf trace_*


.PHONY: record-firefox
help::
	@echo "  make record-firefox"
	@echo "    Record firefox running standalone."
record-firefox:
	$(RR) $(RECORD) $(FF) -no-remote -P garbage


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
		--xre-path=$(FF_DIR)/firefox \
		--verbose \
		$(XPCSHELL) \
		$(XPCSHELL_DIR)/tests/toolkit/components/search/tests/xpcshell


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
