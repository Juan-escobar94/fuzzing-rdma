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


set -euo pipefail
# set up 4 tap devices
#
user=`whoami`
for i in {0..3}
do
   sudo ip tuntap add dev tap$i mode tap user $user 
done

sleep 0.2s

# set up 2 bridges
#
for i in {0..1}
do
   sudo ip link add br$i type bridge
done


sleep 0.2s

# assign IP addresses
#
for i in {1..2}
do
    sudo ip a add 172.0.0.$i/24 dev br$(($i -1)) 
done


sleep 0.2s

# connect tap devices to bridges
#
for i in {0..1}
do
    sudo ip link set tap$i master br$i
done
for i in {2..3}
do
    sudo ip link set tap$i master br$(($i - 2))
done


sleep 0.2s

# bring the interfaces up
#
for i in {0..3}
do
    sudo ip link set dev tap$i up
done
for i in {0..1}
do
    sudo ip link set dev br$i up
done


sleep 0.2s

echo "Done setting up interfaces"
