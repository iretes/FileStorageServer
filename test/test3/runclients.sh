#!/bin/bash

if [ $# -eq 0 ]; then
    echo "usage: $0 id"
    exit 1
fi

if (($1 < 0 || $1 > 9)); then
    echo "id deve appartenere all'intervallo [0,9]"
    exit 1
fi

# Avvio due client che salvano i file espulsi e letti

bin/client  -W test/testfiles/randomfiles/"$1"/randfile1.dat -D test/test3/output/evictedclient"$1" \
    -r test/testfiles/randomfiles/"$1"/randfile1.dat -d test/test3/output/readclient"$1" \
    -l test/testfiles/randomfiles/"$1"/randfile1.dat \
    -c test/testfiles/randomfiles/"$1"/randfile1.dat \
    -R n=4 -d test/test3/output/readclient"$1" \
    -f test/test3/output/storage_socket \
    &> /dev/null

bin/client -w test/testfiles/randomfiles/"$1" -D test/test3/output/evictedclient"$1" \
    -l test/testfiles/randomfiles/"$1"/randfile2.dat \
    -R -d test/test3/output/readclient"$1" \
    -u test/testfiles/randomfiles/"$1"/randfile2.dat \
    -a test/testfiles/randomfiles/"$1"/randfile2.dat,test/testfiles/randomfiles/"$1"/randfile1.dat \
    -f test/test3/output/storage_socket \
    &> /dev/null
    
# Avvio ininterrottamente client che non salvano i file espulsi e letti

while true 
do
    bin/client  -W test/testfiles/randomfiles/"$1"/randfile1.dat \
        -r test/testfiles/randomfiles/"$1"/randfile1.dat \
        -l test/testfiles/randomfiles/"$1"/randfile1.dat \
        -c test/testfiles/randomfiles/"$1"/randfile1.dat \
        -R n=4 \
        -w test/testfiles/subdir1 \
        -r test/testfiles/subdir1/file1.txt \
        -f test/test3/output/storage_socket \
        &> /dev/null
    
    bin/client -w test/testfiles/randomfiles/"$1" \
        -l test/testfiles/randomfiles/"$1"/randfile2.dat \
        -R \
        -u test/testfiles/randomfiles/"$1"/randfile2.dat \
        -a test/testfiles/randomfiles/"$1"/randfile2.dat,test/testfiles/randomfiles/"$1"/randfile1.dat \
        -f test/test3/output/storage_socket \
        &> /dev/null
done