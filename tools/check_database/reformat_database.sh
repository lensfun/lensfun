#!/bin/sh

SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")

for A in "$SCRIPTPATH"/../../data/db/*.xml
do
    XMLLINT_INDENT="    " xmllint --format --encode utf-8 "$A" > /tmp/lensfun_xmllint.xml
    sed 's+\( </\(lens\|mount\|camera\)>\)+\1\n+;s/\(<lensdatabase version=".\+">\)/\1\n/' < /tmp/lensfun_xmllint.xml \
        | sed ':a;N;$!ba;s/<?xml version="1.0" encoding="utf-8"?>\n//' > "$A"
done
