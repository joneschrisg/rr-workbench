#!/bin/bash

rm -rf .ff
mkdir .ff
cd .ff

wget -nd -r -l 1 -A 'firefox*linux-i686.tar.bz2,firefox*linux-i686*.zip' \
    http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-trunk/

tar jxf firefox-*.tar.bz2
unzip -q firefox-*.zip
