#!/bin/bash

if [ ! $# -eq 2 ]; then
    echo "usage: $0 output_dir eviction_policy"
    exit 1
fi

if [ ! -d $1 ]; then
    echo "$1 non è una directory"
    exit 1
fi

CLIENT=1

# Avvio i client per il testing delle politiche di espulsione

if [ "$2" == "FIFO" ]; then
    bin/client -W test/testfiles/randomfiles/randfile4.dat,test/testfiles/randomfiles/randfile5.dat \
                -p \
                -f "$1"/storage_socket \
                &> "$1"/client"$CLIENT"out.txt

    ((CLIENT++))

    # la scrittura del file test/testfiles/randomfiles/randfile3.dat
    # dovrebbe comportare l'espulsione di test/testfiles/randomfiles/randfile4.dat
    bin/client -W test/testfiles/randomfiles/randfile3.dat \
                -p \
                -f "$1"/storage_socket \
                &> "$1"/client"$CLIENT"out.txt

    ((CLIENT++))

elif [ "$2" == "LFU" ]; then
    bin/client -W test/testfiles/randomfiles/randfile2.dat,test/testfiles/randomfiles/randfile3.dat,test/testfiles/randomfiles/randfile4.dat \
                -p \
                -f "$1"/storage_socket \
                &> "$1"/client"$CLIENT"out.txt

    ((CLIENT++))

    # la scrittura del file test/testfiles/randomfiles/randfile5.dat
    # dovrebbe comportare l'espulsione di test/testfiles/randomfiles/randfile3.dat e di test/testfiles/randomfiles/randfile2.dat
    bin/client -r test/testfiles/randomfiles/randfile2.dat,test/testfiles/randomfiles/randfile4.dat \
                -W test/testfiles/randomfiles/randfile5.dat \
                -p \
                -f "$1"/storage_socket \
                &> "$1"/client"$CLIENT"out.txt
    
    ((CLIENT++))

elif [ "$2" == "LRU" ]; then
    bin/client -W test/testfiles/randomfiles/randfile4.dat,test/testfiles/randomfiles/randfile5.dat \
                -p \
                -f "$1"/storage_socket \
                &> "$1"/client"$CLIENT"out.txt

    ((CLIENT++))

    # la scrittura del file test/testfiles/randomfiles/randfile3.dat
    # dovrebbe comportare l'espulsione di test/testfiles/randomfiles/randfile5.dat
    bin/client -r test/testfiles/randomfiles/randfile4.dat \
                -W test/testfiles/randomfiles/randfile3.dat \
                -p \
                -f "$1"/storage_socket \
                &> "$1"/client"$CLIENT"out.txt
    
    ((CLIENT++))

elif [ ! "$2" == "LW" ]; then 
    echo "$2 non è una politica di espulsione valida"
    exit 1
fi

# Avvio altri client

bin/client -W test/testfiles/file1.txt,test/testfiles/file2.txt \
            -r test/testfiles/file1.txt -t 200 -d "$1"/readclient"$CLIENT" \
            -r test/testfiles/file2.txt -t 200 -d "$1"/readclient"$CLIENT" \
            -p \
            -f "$1"/storage_socket \
            &> "$1"/client"$CLIENT"out.txt

((CLIENT++))

bin/client -w test/testfiles/subdir1,n=1 -D "$1"/evictedclient"$CLIENT" \
            -l test/testfiles/file1.txt \
            -R n=0 -t 100 -d "$1"/readclient"$CLIENT" \
            -u test/testfiles/file1.txt \
            -p \
            -f "$1"/storage_socket \
            &> "$1"/client"$CLIENT"out.txt

((CLIENT++))

bin/client -a test/testfiles/file2.txt,test/testfiles/file1.txt -D "$1"/evictedclient"$CLIENT" \
            -p \
            -f "$1"/storage_socket \
            &> "$1"/client"$CLIENT"out.txt

((CLIENT++))

bin/client -w test/testfiles/subdir2 -D "$1"/evictedclient"$CLIENT" \
            -p \
            -f "$1"/storage_socket \
            &> "$1"/client"$CLIENT"out.txt

((CLIENT++))

bin/client -c test/testfiles/randomfiles/randfile1.dat,test/testfiles/randomfiles/randfile2.dat \
            -r test/testfiles/file1.txt,test/testfiles/file2.txt -t 100 -d "$1"/readclient"$CLIENT" \
            -p \
            -f "$1"/storage_socket \
            &> "$1"/client"$CLIENT"out.txt

((CLIENT++))

bin/client -W test/testfiles/randomfiles/randfile5.dat -D "$1"/evictedclient"$CLIENT" \
            -l test/testfiles/randomfiles/randfile5.dat \
            -R n=3 -d "$1"/readclient"$CLIENT" \
            -p \
            -f "$1"/storage_socket \
            &> "$1"/client"$CLIENT"out.txt

((CLIENT++))

bin/client -c test/testfiles/randomfiles/randfile4.dat,test/testfiles/randomfiles/randfile5.dat \
            -R -d "$1"/readclient"$CLIENT" \
            -p \
            -f "$1"/storage_socket \
            &> "$1"/client"$CLIENT"out.txt

((CLIENT++))

# la connessione del seguente cliente dovrebbe essere chiusa dal server
# quando esso tenta di scrivere test/testfiles/randomfiles/randfile6.dat che ha una dimensione maggiore dello storage
bin/client -w test/testfiles -D "$1"/evictedclient"$CLIENT" \
            -p \
            -f "$1"/storage_socket \
            &> "$1"/client"$CLIENT"out.txt

exit 0