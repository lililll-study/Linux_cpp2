#!/bin/bash
for i in {1..254}; do
    ping -c 2 -i 0.5 172.19.104.$i &>/dev/null
    if [ $? -eq 0 ]; then
        echo "172.19.104.$i is up"
    else
        echo "172.19.104.$i is down"
    fi
done