#!/bin/bash

CLIENT=1

bin/client -W test/testfiles/file1.txt,test/testfiles/file.pdf -t 200 \
    -R -d test/test1/output/readclient"$CLIENT" -t 200 \
    -w test/testfiles -t 200 -D test/test1/output/evictedclient"$CLIENT" \
    -a test/testfiles/file2.txt,test/testfiles/file1.txt -t 200 -D test/test1/output/evictedclient"$CLIENT" \
    -r test/testfiles/file1.txt -d test/test1/output/readclient"$CLIENT" -t 200 \
    -l test/testfiles/file1.txt -t 200 \
    -u test/testfiles/file1.txt -t 200 \
    -c test/testfiles/file1.txt \
    -p \
    -f test/test1/output/storage_socket \
    &>test/test1/output/client"$CLIENT"out.txt &

((CLIENT++))

bin/client -W test/testfiles/file1.txt,test/testfiles/file2.txt -D test/test1/output/evictedclient"$CLIENT" -t 200 \
    -l test/testfiles/file1.txt \
    -f test/test1/output/storage_socket \
    -p \
    &>test/test1/output/client"$CLIENT"out.txt

((CLIENT++))

bin/client -h \
    &>test/test1/output/client"$CLIENT"out.txt

exit 0