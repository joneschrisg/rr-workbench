rr-workbench is a collection of helpers designed to make working with
rr and firefox easier.  The helpers expect a work environment set up
something like

    $HOME/rr
      rr         (clone of rr sources)
      workbench  (clone of this repo)

You'll want to use the `workbench` directory as scratch space.  It
knows how to ignore and clean up the `trace_*` directories created by
rr.

Documentation for all helpers is available from

    make help

However, the three most useful targets which aren't going to change,
are

    make clean
      Remove all trace directories.
    make record-mochitests [TEST_PATH=dir]
      Record firefox running the mochitest suite, optionally
      just the TEST_PATH tests.
    make update-firefox [FF_URL=url]
      Blow away the current firefox build and testsuite and
      download the latest from FF_URL (defaulting to nightly
      builds of trunk).
