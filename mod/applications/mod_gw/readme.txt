

yum install git gcc-c++ autoconf automake libtool wget python ncurses-devel zlib-devel libjpeg-devel openssl-devel e2fsprogs-devel  libedit-devel yasm nasm

wget http://www.mega-nerd.com/libsndfile/files/libsndfile-1.0.27.tar.gz
tar xzvf libsndfile-1.0.27.tar.gz
cd libsndfile-1.0.27
./configure && make && make install 
cp /usr/local/lib/pkgconfig/sndfile.pc /usr/lib64/pkgconfig/

wget http://downloads.xiph.org/releases/opus/opus-1.1.2.tar.gz
tar xzvf opus-1.1.2.tar.gz 
cd opus-1.1.2
./configure && make && make install 
cp /usr/local/lib/pkgconfig/opus.pc /usr/lib64/pkgconfig


wget http://prdownloads.sourceforge.net/libpng/libpng-1.6.26.tar.gz?download
mv libpng-1.6.26.tar.gz?download libpng-1.6.26.tar.gz
tar zxvf libpng-1.6.26.tar.gz
cd libpng-1.6.26 
./configure 
./make && make install 
cp /usr/local/lib/pkgconfig/libpng16.pc /usr/lib64/pkgconfig/
cp /usr/local/lib/pkgconfig/libpng.pc /usr/lib64/pkgconfig/


tar zxvf freeswitch.tar.gz 
cd  freeswitch

./configure --disable-SYSTEM_LUA -C



useradd --system --home-dir /usr/local/freeswitch -G daemon freeswitch
passwd -l freeswitch
chown -R freeswitch:daemon /usr/local/freeswitch
chmod -R 770 /usr/local/freeswitch/
chmod -R 750 /usr/local/freeswitch/bin/*
mkdir /var/run/freeswitch
chown -R freeswitch:daemon  /var/run/freeswitch
ln -s /usr/local/freeswitch/bin/freeswitch /usr/bin/
ln -s /usr/local/freeswitch/bin/fs_cli /usr/bin/
cp /usr/src/freeswitch/build/freeswitch.init.redhat  /etc/init.d/freeswitch
chmod 750 /etc/init.d/freeswitch
chown freeswitch:daemon /etc/init.d/freeswitch
chkconfig --add freeswitch && chkconfig --levels 35 freeswitch on

DST=/usr/local/freeswitch
SRC=/usr/src/freeswitch/src
mv $DST/conf $DST/conf.bak
cp -r $SRC/mod/application/mod_ihmp/conf $DST

service freeswitch start 

cat >>  /etc/security/limits.conf << EOF
freeswitch      soft    nofile          65536
freeswitch      hard    nofile          65536
freeswitch      soft    nproc           4096
freeswitch      hard    nproc           4096
EOF

#setting

#log level 

#conf/autoload_configs/switch.conf.xml 
#    <param name="loglevel" value="debug"/>

#conf/var.xml 
#    <X-PRE-PROCESS cmd="set" data="console_loglevel=info"/>

support g729 

https://code.google.com/archive/p/fsg729/source/default/source

mod/codec/mod_com_g729


# copy to octon and build 

#!/bin/bash
DHOST=octon
DPATH=/usr/src/freeswitch/src/mod/applications/mod_ihmp/
echo "COPY ihmp.c to ${DHOST}" 
scp /home/wangkan/work/octon/freeswitch/src/mod/applications/mod_ihmp/mod_ihmp.c ${DHOST}:${DPATH}
ssh ${DHOST} "cd ${DPATH};make;cp -f .libs/mod_ihmp.la /usr/local/freeswitch/mod/;cp -f .libs/mod_ihmp.so /usr/local/freeswitch/mod/;/etc/init.d/freeswitch restart;"
#echo "Watch freeswitch log"
#ssh ${DHOST} "tail -f /usr/local/freeswitch/log/freeswitch.log"


#create install package

ssh root@172.16.5.168
password: octon168 

scp 192.168.0.103:/usr/local/freeswitch/mod/* /mmcc/imedia2/mod/
cd /mmcc    
rm -f imedia2.tar.gz
tar czf imedia2.tar.gz imedia2/
cat install.sh imedia2.tar.gz > /root/imedia2.bin

rm -f /mmcc/imedia2.tar.gz;tar czf imedia2.tar.gz imedia2/;cat install.sh imedia2.tar.gz > /root/imedia2.bin


./sipp -sf uac2.xml -i 192.168.56.1 -m 1 -aa -trace_msg  192.168.56.101:5070


172.16.5.168 => QA test host 

c.sh 
ssh octon "scp /usr/local/freeswitch/mod/mod_ihmp.* 172.16.5.168:/mmcc/imedia2/mod/"
ssh 172.16.5.168 
/etc/init.d/freeswitch restart # for test 


local_ip_v4=ip addr show eth0 | awk '/inet /{print $2}' | head -n 1 | cut -d '/' -f 1


update soa_static.c line 819  
    >>
    s_media[i] = soa_sdp_make_rejected_media(home, rm, session, 0);
    <<
    s_media[i] = soa_sdp_make_rejected_media(home, rm, session, 1);

fixbug: MII-31 https://cm.octon.net/jira/browse/MII-31 

copy libs/sofia-sip/libsofia-sip-ua/soa/soa_static.c remote 
make sofia-reconf
make
make intall 
 