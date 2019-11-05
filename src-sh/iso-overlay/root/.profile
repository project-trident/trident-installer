_tty=`tty`
if [ "${_tty}" = "/dev/tty1" ] ; then
  exec start-trident-installer
fi
