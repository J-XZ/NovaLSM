#!/bin/bash

# cloudlab节点列表
node_list=("clnode277" "clnode283")

node_list_size=${#node_list[@]}

END=$node_list_size

REMOTE_HOME="/proj/bg-PG0"
setup_script="$REMOTE_HOME/haoyu/scripts/env"
limit_dir="$REMOTE_HOME/haoyu/scripts"
LOCAL_HOME="/Users/haoyuh/Documents/PhdUSC/dblab/nova/NovaLSM/scripts/bootstrap"
host="Nova.bg-PG0.apt.emulab.net"
# host="Nova.bg-PG0.utah.cloudlab.us"

# scp -r $LOCAL_HOME/* haoyu@node-0.${host}:/proj/bg-PG0/haoyu/scripts/

for ((i = 0; i < END; i++)); do
    current_node=${node_list[$i]}
    echo "building server on node $current_node"
    # ssh -oStrictHostKeyChecking=no haoyu@node-$i.${host} "bash $setup_script/setup-ssh.sh"
done

echo ""

for ((i = 0; i < END; i++)); do
    current_node=${node_list[$i]}
    echo "building server on node $current_node"
    # ssh -oStrictHostKeyChecking=no haoyu@node-$i.${host} "sudo cp $limit_dir/ulimit.conf /etc/systemd/user.conf"
    # ssh -oStrictHostKeyChecking=no haoyu@node-$i.${host} "sudo cp $limit_dir/sys_ulimit.conf /etc/systemd/system.conf"
    # ssh -oStrictHostKeyChecking=no haoyu@node-$i.${host} "sudo cp $limit_dir/limit.conf /etc/security/limits.conf"
    # ssh -oStrictHostKeyChecking=no haoyu@node-$i.${host} "sudo reboot"
done
