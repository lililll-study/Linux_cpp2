#!/bin/bash

echo "hello world"

lhy="www.0voice.com"
echo $lhy

for file in $(ls /home/lhy/share/); do
	echo "${file}"
done
