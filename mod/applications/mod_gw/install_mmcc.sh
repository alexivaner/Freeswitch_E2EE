#!/bin/bash 
export user=$(whoami)

rootjob()
{
echo "Is Root, Do something"
echo "Install all *.deb..."
dpkg -i /mmcc/imedia2/depend/*.deb

echo "link *.so files..."
rm -f /usr/lib/libfreeswitch.so
rm -f /usr/lib/libfreeswitch.so.1
rm -f /usr/lib/libfreeswitch.so.1.0.0
ln -s /mmcc/imedia2/lib/libfreeswitch.so /usr/lib/
ln -s /mmcc/imedia2/lib/libfreeswitch.so.1 /usr/lib/
ln -s /mmcc/imedia2/lib/libfreeswitch.so.1.0.0 /usr/lib/

rm -f /usr/lib/libsofia-sip-ua-glib.so
rm -f /usr/lib/libsofia-sip-ua-glib.so.3
rm -f /usr/lib/libsofia-sip-ua-glib.so.3.0.0
ln -s /mmcc/imedia2/lib/libsofia-sip-ua-glib.so /usr/lib/
ln -s /mmcc/imedia2/lib/libsofia-sip-ua-glib.so.3 /usr/lib/
ln -s /mmcc/imedia2/lib/libsofia-sip-ua-glib.so.3.0.0 /usr/lib/

rm -f /usr/lib/libsofia-sip-ua.so
rm -f /usr/lib/libsofia-sip-ua.so.0
rm -f /usr/lib/libsofia-sip-ua.so.0.6.0
ln -s /mmcc/imedia2/lib/libsofia-sip-ua.so /usr/lib/
ln -s /mmcc/imedia2/lib/libsofia-sip-ua.so.0 /usr/lib/
ln -s /mmcc/imedia2/lib/libsofia-sip-ua.so.0.6.0 /usr/lib/

rm -f /usr/lib/libspandsp.so
rm -f /usr/lib/libspandsp.so.3
rm -f /usr/lib/libspandsp.so.3.0.0
ln -s /mmcc/imedia2/lib/libspandsp.so /usr/lib/
ln -s /mmcc/imedia2/lib/libspandsp.so.3 /usr/lib/
ln -s /mmcc/imedia2/lib/libspandsp.so.3.0.0 /usr/lib/

echo "Create octon imedia2 env2..."
if [ ! -d /mmcc/ssl ];then
    echo "mkdir /mmcc/ssl"
    mkdir -p /mmcc/ssl
    chown -R mmccadmin:mmccadmin /mmcc/ssl
fi
if [ ! -d /etc/opt/octon ];then
    echo "mkdir /etc/opt/octon"
    mkdir -p /etc/opt/octon
    cp /mmcc/imedia2/tools/logrotate.conf /etc/opt/octon/
    cp /mmcc/imedia2/tools/logrotate /etc/opt/octon/
fi
if [ ! -d /etc/opt/octon/logrotate.d ];then
    echo "mkdir /etc/opt/octon/logrotate.d"
    mkdir -p /etc/opt/octon/logrotate.d
fi
cp /mmcc/imedia2/tools/octon-imedia2 /etc/opt/octon/logrotate.d/
chown root:root /etc/opt/octon/logrotate.d/octon-imedia2
cp /mmcc/imedia2/tools/imedia2.service /etc/systemd/system
chown mmccadmin:daemon /etc/systemd/system/imedia2.service
systemctl daemon-reload
systemctl enable imedia2.service

T=$(grep mmccadmin /etc/security/limits.conf | wc -l)
if [ ${T} -eq 0 ];then
    echo "limits.conf modified"
    cat >>  /etc/security/limits.conf << EOF
mmccadmin      soft    nofile          65536
mmccadmin      hard    nofile          65536
mmccadmin      soft    nproc           4096
mmccadmin      hard    nproc           4096
EOF
fi
}

echo "user is $user"
isroot=0
String1="root"
if [[ "$user" == "$String1" ]];then
   isroot=1
fi

LINES=`awk '{if($0=="exit 0"){print NR+1;nextfile;}}' $0`
echo "Octon imedia2 extact and copy to /mmcc"
tail -n +${LINES} $0 > /tmp/imedia2.tar.gz

T=$( grep mmccadmin /etc/passwd | wc -l )
if [ ${T} -eq 0 ];then
    /usr/sbin/groupadd -g 503 mmccadmin ; /usr/sbin/useradd -g 503 -u 503 -p mmccadmin -m mmccadmin ; 
    passwd -l mmccadmin
    gpasswd -a mmccadmin daemon
fi

if [ ! -d /mmcc ];then
    echo "mkdir /mmcc"
    mkdir -p /mmcc
    chown -R mmccadmin:mmccadmin /mmcc
fi

if [ -d /mmcc/imedia2/conf ];then
    echo "backup config files"
    cp -p -r /mmcc/imedia2/conf /mmcc/imedia2/conf.backup
fi
 
rm -f /mmcc/imedia2/zzz*
echo "Create octon imedia2 env1..."
tar zxf /tmp/imedia2.tar.gz -C /mmcc

file="/mmcc/imedia2/conf/imedia.xml"
if [ ! -f "$file" ]; then
   echo "Create imedia.xml file"
   cp /mmcc/imedia2/tools/imedia.xml /mmcc/imedia2/conf
fi

chown -R mmccadmin:mmccadmin  /mmcc/imedia2
chmod -R 770 /mmcc/imedia2
chmod -R 750 /mmcc/imedia2/bin/*
mkdir -p /var/run/imedia
chown -R mmccadmin:mmccadmin  /var/run/imedia
mkdir -p /var/opt/mmcc/imedia2/log
chown -R mmccadmin:mmccadmin /var/opt/mmcc/imedia2
if [ $isroot -eq 1 ]; then
    rootjob
fi
rm -f /tmp/imedia2.tar.gz
echo "Install finish"
echo "servicectl start imedia2.service => to start imedia2"
exit 0
