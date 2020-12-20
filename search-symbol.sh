#! /bin/bash
# This shellscript find all shared object which contains the specified
# symbol.

if [ -z $1 ]
then
    echo You must specify the name of the symbol.
    exit 1
fi

sym=$1
sos=`ldconfig -p | awk -F '=>' '{print $2}' | grep so | sort | uniq`

for s in ${sos}
do
    PRE_IFS=$IFS
    IFS=$'\n'
    for l in `readelf -s $s | grep -e ${sym} | grep -v UND`
    do
        echo ${l} in ${s}
    done
    IFS=$PRE_IFS
done
