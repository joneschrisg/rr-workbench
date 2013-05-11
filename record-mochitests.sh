#!/bin/bash

RR="../rr"
FF=".ff"

OBJ="$RR/obj"
LOG="$FF/mochitest.log"
MOCHITEST_DIR="$FF/mochitest"

$OBJ/bin/rr --record \
    --filter_lib="$OBJ/lib/librr_wrap_syscalls.so" \
    $FF/firefox/firefox -no-remote -P garbage

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
