RR_DIR ?= ../rr
FF_DIR ?= .ff
FF_URL ?= http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-trunk/

OBJDIR = $(RR_DIR)/obj
LOG = $(FF_DIR)/mochitest.log
MOCHITEST_DIR = $(FF_DIR)/mochitest

RR = "$(OBJDIR)/bin/rr"
LIB = "$(OBJDIR)/lib/librr_wrap_syscalls.so"

FF = "$(FF_DIR)/firefox/firefox"

# Remove all traces.
.PHONY: clean
clean:
	rm -rf trace_*


# Record the firefox mochitest suite.
.PHONY: record-mochitests
record-mochitests:
#	$(RR) --record $(FF) -no-remote -P garbage
	$(RR) --record --filter_lib=$(LIB) $(FF) -no-remote -P garbage

# rm -f $LOG
# python  $MOCHITEST_DIR/runtests.py --autorun --close-when-done \
#     --console-level=INFO --log-file=$LOG --file-level=INFO \
#     --testing-modules-dir=$ROOT/modules \
#     --extra-profile-file=$ROOT/bin/plugins \
#     $(SYMBOLS_PATH) $(TEST_PATH_ARG) $(EXTRA_TEST_ARGS)

# $(call check_test_error_internal,"To rerun your failures please run 'make $@-rerun-failures'")
#   @errors=`grep "TEST-UNEXPECTED-" $@.log` ;\
#   if test "$$errors" ; then \
# 	  echo "$@ failed:"; \
# 	  echo "$$errors"; \
#           $(if $(1),echo $(1)) \
# 	  exit 1; \
#   fi


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
