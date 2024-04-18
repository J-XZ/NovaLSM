#!/bin/bash

# set -x

user_name=ruixuan

END=$1
REMOTE_HOME="/users/${user_name}/NovaLSM"
setup_script="$REMOTE_HOME/scripts/env"
limit_dir="$REMOTE_HOME/scripts"
LOCAL_HOME="/Users/jiangxuzhen/Downloads/e-lsmtree/NovaLSM/scripts/bootstrap"

host="xz.dpmfs-pg0.wisc.cloudlab.us"

rm -r ./tmp_pubkeys/
mkdir -p ./tmp_pubkeys
for ((i=0;i<END;i++)); do
    echo "building server on node $i"
    echo "node: ${user_name}@node$i.${host}"
    echo "rsync -avzP $LOCAL_HOME/* ${user_name}@node$i.${host}:${limit_dir}/"
    rsync -avzP $LOCAL_HOME/* ${user_name}@node$i.${host}:${limit_dir}/
    ssh -oStrictHostKeyChecking=no ${user_name}@node$i.${host} "bash $setup_script/setup-ssh.sh"
    # copy public key to local
    rsync -avzP ${user_name}@node$i.${host}:~/.ssh/id_rsa.pub ./tmp_pubkeys/node$i.pub
done

echo ""

for ((i=0;i<END;i++)); do
    echo "building server on node $i"
    ssh -oStrictHostKeyChecking=no ${user_name}@node$i.${host} "sudo cp $limit_dir/ulimit.conf /etc/systemd/user.conf"
    ssh -oStrictHostKeyChecking=no ${user_name}@node$i.${host} "sudo cp $limit_dir/sys_ulimit.conf /etc/systemd/system.conf"
    ssh -oStrictHostKeyChecking=no ${user_name}@node$i.${host} "sudo cp $limit_dir/limit.conf /etc/security/limits.conf"
    ssh -oStrictHostKeyChecking=no ${user_name}@node$i.${host} "sudo reboot"
done
