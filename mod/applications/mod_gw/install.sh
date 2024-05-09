#!/bin/bash 
LINES=`awk '{if($0=="exit 0"){print NR+1;nextfile;}}' $0`
echo "Octon imedia2 extact and copy to /mmcc"
tail -n +${LINES} $0 > /tmp/imedia2.tar.gz

if [ ! -d /mmcc ];then 
    echo "mkdir /mmcc"
    mkdir -p /mmcc
fi 
T=$( grep mmccadmin /etc/passwd | wc -l )
if [ ${T} -eq 0 ];then 
    addgroup mmccadmin
    useradd mmccadmin 
    passwd -l mmccadmin
    gpasswd -a mmccadmin daemon
    chown -R mmccadmin:mmccadmin  /mmcc
fi 

tar zxf /tmp/imedia2.tar.gz -C /mmcc
echo "Create octon imedia2 env..."
#T=$( yum list installed | grep speex | wc -l )
#if [ ${T} -eq 0 ];then
#   echo "Install speex library"
#   yum -q -y localinstall /mmcc/imedia2/depend/speex-devel-1.2-0.12.rc1.1.el6.x86_64.rpm
#fi
file="/mmcc/imedia2/conf/imedia.xml"
if [ ! -f "$file" ]; then
   echo "Creating imedia.xml file"
   cp /mmcc/imedia2/tools/imedia.xml /mmcc/imedia2/conf
fi
chown -R mmccadmin:mmccadmin  /mmcc/imedia2
chmod -R 770 /mmcc/imedia2
chmod -R 750 /mmcc/imedia2/bin/*
mkdir -p /var/run/imedia
chown -R mmccadmin:mmccadmin  /var/run/imedia
mkdir -p /var/opt/mmcc/imedia2/log
chown -R mmccadmin:mmccadmin /var/opt/mmcc/imedia2
#rm -f /usr/bin/imedia
#rm -f /usr/bin/fs_cli
#ln -s /mmcc/imedia2/bin/imedia /usr/bin
#ln -s /mmcc/imedia2/bin/fs_cli /usr/bin
rm -f /usr/lib64/libfreeswitch.so*
#ln -s /mmcc/imedia2/lib/libfreeswitch.so /usr/lib64/
#ln -s /mmcc/imedia2/lib/libfreeswitch.so.1 /usr/lib64/
#ln -s /mmcc/imedia2/lib/libfreeswitch.so.1.0.0 /usr/lib64/
rm -f /usr/lib/libfreeswitch.so
rm -f /usr/lib/libfreeswitch.so.1
rm -f /usr/lib/libfreeswitch.so.1.0.0
ln -s /mmcc/imedia2/lib/libfreeswitch.so /usr/lib/
ln -s /mmcc/imedia2/lib/libfreeswitch.so.1 /usr/lib/
ln -s /mmcc/imedia2/lib/libfreeswitch.so.1.0.0 /usr/lib/
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
chown mmccadmin:mmccadmin /etc/opt/octon/logrotate.d/octon-imedia2
#cp /mmcc/imedia2/imedia2.init.redhat /etc/init.d/imedia
#chmod 750 /etc/init.d/imedia
#chown mmccadmin:daemon /etc/init.d/imedia
#chkconfig --add imedia && chkconfig --levels 35 imedia on
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
echo "Install finish"
echo "servicectl start imedia2.service => to start imedia2"

exit 0
