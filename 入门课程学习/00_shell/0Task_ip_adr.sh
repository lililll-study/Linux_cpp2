#!/bin/bash
local_ip=$(hostname -I | awk '{print $1}')
echo "Local IP Address: $local_ip"


