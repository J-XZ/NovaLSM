#!/bin/bash

user_name=ruixuan
basedir="/users/${user_name}/NovaLSM"
numServers=$1
prefix="h"
host="xz.dpmfs-pg0.wisc.cloudlab.us"

for ((i = 0; i < numServers; i++)); do
	echo "*******************************************"
	echo "*******************************************"
	echo "******************* node$i ********************"
	echo "*******************************************"
	echo "*******************************************"
	echo node$i.${host}
	ssh -oStrictHostKeyChecking=no node$i "sudo apt-get update"
	ssh -oStrictHostKeyChecking=no node$i "sudo apt-get --yes install screen"
	ssh -n -f -oStrictHostKeyChecking=no node$i screen -L -S env1 -dm "$basedir/scripts/env/setup-all.sh"
	echo "ssh -n -f -oStrictHostKeyChecking=no node$i screen -L -S env1 -dm \"$basedir/scripts/env/setup-all.sh\""
done

# ssh -n -f -oStrictHostKeyChecking=no node0.xz.dpmfs-pg0.wisc.cloudlab.us screen -L -S env1 -dm "/users/ruixuan/NovaLSM/scripts/env/setup-all.sh"
# ssh -n -f -oStrictHostKeyChecking=no node1.xz.dpmfs-pg0.wisc.cloudlab.us screen -L -S env1 -dm "/users/ruixuan/NovaLSM/scripts/env/setup-all.sh"

sleep 10

sleepcount="0"

for ((i = 0; i < numServers; i++)); do
	while ssh -oStrictHostKeyChecking=no node$i "screen -list | grep -q env1"; do
		((sleepcount++))
		sleep 10
		echo "waiting for node$i "
	done
done

echo "init env took $((sleepcount / 6)) minutes"
