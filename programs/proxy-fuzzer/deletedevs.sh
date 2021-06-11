#!/usr/bin/env bash
set -x

# make sure the devices dont exist already
#
for i in {0..3}
do
   sudo ip link del tap$i
done

sleep 0.2s

for i in {0..1}
do
   sudo ip link del br$i
done

sleep 0.2s
