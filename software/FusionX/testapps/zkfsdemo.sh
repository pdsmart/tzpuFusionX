mkdir -p /var/tmp
#ifconfig lo 127.0.0.1 up
#ifconfig eth0 172.19.24.186 netmask 255.255.255.0
#ifconfig eth0 hw ether 00:70:27:00:00:01
#route add default gw 172.19.24.254
/customer/browser/run.sh &
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib:/customer/lib:/customer/libdns:/customer/libsecurity:/lib:/config/wifi
cd /customer
chmod 777 MyPlayer
./MyPlayer &
chmod 777 zkgui
./zkgui &
