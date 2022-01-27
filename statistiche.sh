#!/bin/bash

if [ $# -eq 0 ]
  then
    echo "usage: $0 logfile"
    exit 1
fi

LOGFILE=$1
if [ ! -f "$LOGFILE" ]; 
  then
    echo "Specifica un file regolare esistente"
    exit 1
fi

echo "========================= OPERAZIONI ========================="

awk -F"," '$3 ~ /OPEN_NO_FLAGS/ {SUM+=1} END {print "Numero di open senza flag: " SUM+0}' $LOGFILE

awk -F"," '$3 ~ /OPEN_LOCK/ {SUM+=1} END {print "Numero di open-lock: " SUM+0}' $LOGFILE

awk -F"," '$3 ~ /OPEN_CREATE_LOCK/ {SUM+=1} END {print "Numero di open-create-lock: " SUM+0}' $LOGFILE

awk -F"," '$3 ~ /^LOCK$/ {SUM+=1} END {print "Numero di lock: " SUM+0}' $LOGFILE

awk -F"," '$3 ~ /UNLOCK/ {SUM+=1} END {print "Numero di unlock: " SUM+0}' $LOGFILE

awk -F"," '$3 ~ /^CLOSE$/ {SUM+=1} END {print "Numero di close: " SUM+0}' $LOGFILE

awk -F"," '$3 ~ /READ|READN/ {SUM+=1} END {print "Numero di file letti: " SUM+0}' $LOGFILE

awk -F"," '$3 ~ /WRITE|APPEND/ {SUM+=1} END {print "Numero di file scritti (con write o append): " SUM+0}' $LOGFILE

awk -F"," '$3 ~ /REMOVE/ {SUM+=1} END {print "Numero di remove: " SUM+0}' $LOGFILE

awk -F"," '$3 ~ /EVICTION/ {SUM+=1} END {print "Numero di esecuzioni dell'\''algoritmo di rimpiazzamento: " SUM+0}' $LOGFILE

echo "========================= STATISTICHE ========================"

awk -F"," '$3 ~ /READ/ \
{SUM+=$7; COUNT+=1} END {printf "Byte letti in media: %.f\n", (COUNT==0) ? 0 : ((SUM/COUNT)+0.5)}' $LOGFILE

awk -F"," '$3 ~ /APPEND|WRITE/ \
{SUM+=$7; COUNT+=1} END {printf "Byte scritti in media: %.f\n", (COUNT==0) ? 0 : ((SUM/COUNT)+0.5)}' $LOGFILE

echo "Massimo numero di MB memorizzati: \
$(echo "scale=6 ; $(awk -F"," 'NR>1 && $9>max {max=$9} END {print max+0}' $LOGFILE) / 1000000" | bc)"

awk -F"," 'NR>1 && $8 > max {max=$8} END {print "Massimo numero di file memorizzati: " max+0}' $LOGFILE

awk -F"," 'NR>1 && $10 > max {max=$10} END {print "Massimo numero di connessioni contemporanee: " max+0}' $LOGFILE

awk -F"," '$3 ~ /NEW_CONNECTION/ {SUM+=1} END {print "Numero di clienti serviti in totale: " SUM+0}' $LOGFILE

echo "=========================== THREADS =========================="

awk -F"," 'NR>1 &&
$3 ~ /OPEN|WRITE|APPEND|LOCK|UNLOCK|REMOVE|^CLOSE$|READN 1\/|^READ$|TEMPORARILY_UNAVAILABLE/ \
{match_found=1; count[$2]++} 
END {
  if (match_found == 0) 
    print "I thread non hanno servito alcuna richiesta"
  else
    for (i in count) print "Il thread " i " ha servito " count[i] " richieste"  | "sort"
}' $LOGFILE

exit 0