#!/bin/sh

# This file normalises the Lensfun database XML files in place.  It must be
# called from within the DB directory.  It needs xmllint to be installed.

for A in *.xml
do
    XMLLINT_INDENT="    " xmllint --format --encode utf-8 "$A" > /tmp/lensfun_xmllint.xml
    sed 's+\( </\(lens\|mount\|camera\)>\)+\1\n+;s/\(<lensdatabase version=".\+">\)/\1\n/' < /tmp/lensfun_xmllint.xml \
        | sed ':a;N;$!ba;s/<?xml version="1.0" encoding="utf-8"?>\n//' > "$A"
done
