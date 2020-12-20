#! /bin/bash
# List up all relocation entries for the given shared object.
# When you give the second argument, this script filter the output with it.
# ./relocation-check.sh SHARED_OBJECT [RELOCATION_ENTRY]

depends=$(ldd $1 | awk 'NR>1' | awk '{print $3}')

for d in $1 ${depends}
do
    if [ -z $2 ]
    then
        relos=$(readelf -r ${d})
    else
        relos=$(readelf -r ${d} | grep $2)
    fi
    IFS=$'\n'
    for r in ${relos}
    do
        echo ${d} ${r}
    done
done
