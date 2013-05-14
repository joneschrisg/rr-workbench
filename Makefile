RR_DIR ?= ../rr
FF_DIR ?= .ff
FF_URL ?= http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-trunk/

OBJDIR = $(RR_DIR)/obj
LOG = $(FF_DIR)/mochitest.log
MOCHITEST_DIR = $(FF_DIR)/mochitest

RR = "$(OBJDIR)/bin/rr"
LIB = "$(OBJDIR)/lib/librr_wrap_syscalls.so"

FF = "$(FF_DIR)/firefox/firefox"

RECORD = "--record" #--filter_lib=$(LIB)

ifdef TEST_PATH
TEST_PATH_ARG := --test-path="$(TEST_PATH)"
endif

# Remove all traces.
.PHONY: clean
clean:
	rm -rf trace_*


# Record firefox on its own.
.PHONY: record-firefox
record-firefox:
	$(RR) $(RECORD) $(FF) -no-remote -P garbage


# Record firefox running the mochitest suite.
.PHONY: record-mochitests
record-mochitests:
	python "$(MOCHITEST_DIR)/runtests.py" \
		--debugger=$(RR) --debugger-args=$(RECORD) \
		--appname=$(FF) \
		--utility-path="$(FF_DIR)/bin" \
		--extra-profile-file="$(FF_DIR)/bin/plugins" \
		--certificate-path="$(FF_DIR)/certs" \
		--autorun --close-when-done --console-level=INFO \
		$(TEST_PATH_ARG)


# Blow away the current firefox build and testsuite and download the
# latest from $FF_URL.
.PHONY: update-firefox
update-firefox:
	rm -rf $(FF_DIR)
	mkdir $(FF_DIR)
	cd $(FF_DIR) && \
		wget -nd -r -l 1 \
			-A 'firefox*linux-i686.tar.bz2,firefox*linux-i686*.zip' \
			$(FF_URL) && \
		tar jxf firefox-*.tar.bz2 && \
		unzip -q firefox-*.zip
