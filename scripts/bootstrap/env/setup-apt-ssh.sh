#!/bin/bash

# cloudlab节点列表
node_list=("clnode277")
# node_list=("clnode277" "clnode283")

node_list_size=${#node_list[@]}

END=$node_list_size

REMOTE_HOME="/users/ruixuan/NovaLSM"
setup_script="$REMOTE_HOME/scripts/env"
limit_dir="$REMOTE_HOME/scripts"

LOCAL_HOME="/Users/jiangxuzhen/Downloads/e-lsmtree/NovaLSM/scripts/bootstrap"

# ssh ruixuan@clnode277.clemson.cloudlab.us

# host="Nova.bg-PG0.apt.emulab.net"
host="clemson.cloudlab.us"
# host="Nova.bg-PG0.utah.cloudlab.us"

# scp -r $LOCAL_HOME/* haoyu@node-0.${host}:/proj/bg-PG0/haoyu/scripts/

for ((i = 0; i < END; i++)); do
    current_node=${node_list[$i]}
    echo "building server on node $current_node"
    echo "ssh -oStrictHostKeyChecking=no ruixuan@$current_node.${host} \"bash $setup_script/setup-ssh.sh\""
    # ssh -oStrictHostKeyChecking=no ruixuan@$current_node.${host} "bash $setup_script/setup-ssh.sh"
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
