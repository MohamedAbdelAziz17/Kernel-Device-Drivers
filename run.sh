#1/bin/bash

case $1 in

insert)
make sudo insmod main.ko
sudo chmod 777 /dev/myPWM

;;

remove)
sudo rmmod main
make clean

;;

test)
sudo rmmod main
make clean
make
sudo insmod main.ko
sudo chmod 777 /dev/myPWM
dmesg


;;

esac