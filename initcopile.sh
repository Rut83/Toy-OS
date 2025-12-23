gcc -static -O2 -Wall -o init init.c
sudo mount rootfs.img rootfs
sudo rm -f rootfs/sbin/init
sudo cp init rootfs/sbin/init
sudo chmod +x rootfs/sbin/init
sync
sudo umount rootfs