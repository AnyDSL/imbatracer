#!/bin/bash

t=1
while [ $t -le 60 ]
do
    # measure the current cpu utilization.
    top -b -n 1 | awk 'NR > 7 { sum += $9 } END { print sum }'
    sleep 0.1
    ((t++))
done