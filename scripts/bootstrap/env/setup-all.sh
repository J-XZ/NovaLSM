#!/bin/bash

ssd_device=/dev/nvme0n1
user_name=ruixuan
basedir="/users/${user_name}/NovaLSM"

iface=ens2f0np0

export DEBIAN_FRONTEND=noninteractive

bash $basedir/scripts/env/setup-rdma.sh

# nic=$(netstat -i)
# nic=$(ifconfig | grep -oE '^[^ ]+' | grep -v 'lo' | sed 's/[:].*//')
# echo "$nic"
# IFS=$'\n' read -r -d '' -a array <<<"$nic"
# echo "${array[0]}"
# echo "${#array[@]}"
# iface=""
# for element in "${array[@]}"; do
#     if [[ $element == *"enp"* ]]; then
#         echo "awefwef:$element"
#         iface=$element
#     fi
# done
# echo $iface

sudo bash $basedir/scripts/env/nic.sh -i $iface
sudo bash $basedir/scripts/env/sysctl.sh

sudo mkfs.ext4 $ssd_device
sudo mkdir /db
sudo mount $ssd_device /db
sudo chmod 777 /db

# sudo mkfs.ext4 /dev/sda4
# sudo mkdir /db
# sudo mount /dev/sda4 /db
# sudo chmod 777 /db

sudo mkdir /mnt/ramdisk
sudo mount -t tmpfs -o rw,size=20G tmpfs /mnt/ramdisk
sudo chmod 777 /mnt/ramdisk

# cd $basedir/gflags && sudo make install

sudo su -c "echo 'logfile /tmp/screenlog' >> /etc/screenrc"

# Enable gdb history
echo 'set history save on' >>~/.gdbinit && chmod 600 ~/.gdbinit

cd /tmp/
wget https://github.com/fmtlib/fmt/releases/download/6.1.2/fmt-6.1.2.zip
unzip fmt-6.1.2.zip
cd fmt-6.1.2/ && sudo cmake . && sudo make -j64 && sudo make install

cd /tmp/
wget https://github.com/Kitware/CMake/releases/download/v3.13.3/cmake-3.13.3.tar.gz
tar -xf cmake-3.13.3.tar.gz
cd cmake-3.13.3 && sudo ./bootstrap && sudo make -j64 && sudo make install

# cd /tmp/
# wget https://prdownloads.sourceforge.net/oprofile/oprofile-1.3.0.tar.gz
# tar -xf oprofile-1.3.0.tar.gz
# cd oprofile-1.3.0 && ./configure && make -j100 && sudo make install

# operf /path/to/mybinary
# opreport --symbols

rm -rf /tmp/YCSB-Nova

# cp -r "$basedir/YCSB-Nova/" /tmp/

cp -r "$basedir/../NovaLSM-YCSB-Client/" /tmp/
mv /tmp/NovaLSM-YCSB-Client /tmp/YCSB-Nova

cd /tmp/YCSB-Nova

mvn -pl com.yahoo.ycsb:jdbc-binding -am clean package -DskipTests
