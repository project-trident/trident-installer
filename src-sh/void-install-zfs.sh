#!/bin/bash


exit_err(){
  if [ $1 -ne 0 ] ; then
    echo "[ERROR] $2"
    exit 1
  fi
}

#Main setting - just pick a disk
DISK=""
clear
while [ -z "${DISK}" ]
do
  echo "-------------------"
  sfdisk -l | grep "Disk /dev/" | grep -v "/loop" | cut -d , -f 1 | cut -d / -f 3-
  echo "-------------------"
  echo "Type the name of the disk to use (\"sda\" for example)], followed by [ENTER]: "
  read -p "Disk: " DISK
  if [ $? -ne 0 ] ; then exit 1 ; fi
  if [ ! -e "/dev/${DISK}" ] ; then
    echo "Invalid Disk: ${DISK}"
    DISK=""
  else
    # Confirm that the user wants to destroy all the current contents of this disk
    echo "WARNING: This will destroy all current contents on this disk."
    read -p "Type \"yes\" to proceed: " CONFIRM
    if [ "yes" = "${CONFIRM,,}" ] ; then
      DISK="/dev/${DISK}"
    else
      #Return to selection
      DISK=""
    fi
  fi
done

# This script was influenced by https://wiki.voidlinux.org/Manual_install_with_ZFS_root
HOSTNAME="tri-void"
SYSTEMDRIVE="${DISK}2"
BOOTDRIVE="${DISK}1"
BOOTDEVICE="${DISK}"
ZPOOL="trident"
REPO="http://alpha.de.repo.voidlinux.org/current/musl"
PACKAGES=""
PACKAGES_CHROOT="iwd wpa_supplicant dhcpcd bluez linux-firmware foomatic-db-nonfree vlc phototonic trojita telegram-desktop falkon lynx qterminal openvpn git pianobar w3m ntfs-3g fuse-exfat simple-mtpfs fish-shell zsh x264 libdvdcss gutenprint foomatic-db hplip tor nano xorg lumina dhclient"
SERVICES_ENABLED="dbus sshd dhcpcd dhclient cupsd wpa_supplicant"
MNT="/mnt"
CHROOT="chroot ${MNT}/"
## Some important packages
## intel-ucode ?

if [ ! -e "/bin/zpool" ] ; then
  #Need to install the zfs package first
  xbps-install -S
  xbps-install -y zfs
fi

echo "Formatting the disk: ${DISK}"
sfdisk -w always ${DISK} << EOF
	label: gpt
	,100M,U,*
	;
EOF

exit_err $? "Could not partition the disk: ${DISK}"

# Setup the void tweaks for ZFS 
# Steps found at: https://github.com/nightah/void-install
xbps-reconfigure -a
modprobe zfs
exit_err $? "Could not verify ZFS module"

ip link sh | grep ether | cut -d ' ' -f 6 >> /etc/hostid

echo "Create the pool"
echo "zpool create -f -o ashift=12 <pool_name> /dev/sda2"
zpool create -f ${ZPOOL} $SYSTEMDRIVE
exit_err $? "Could not create pool: ${ZPOOL} on ${SYSTEMDRIVE}"
echo
echo "Create a fs for the root file systems:"
echo "zfs create  <pool_name>/ROOT"
zfs create  -o mountpoint=none ${ZPOOL}/ROOT
exit_err $? "Could not create ROOT dataset"

echo
echo "Create a fs for the Void file system"
echo "zfs create <pool_name>/ROOT/<pool_name>"
zfs create -o mountpoint=/ ${ZPOOL}/ROOT/void
exit_err $? "Could not create ROOT/void dataset"

echo "zpool set bootfs=rpool/ROOT/voidlinux_1 <pool_name>"
zpool set bootfs=${ZPOOL}/ROOT/void ${ZPOOL}
exit_err $? "Could not set ROOT/void dataset as bootfs"

echo "Verify pool can be exported/imported"
zpool export ${ZPOOL}
exit_err $? "Could not export pool"
zpool import -R ${MNT} ${ZPOOL}
exit_err $? "Could not import the new pool at ${MNT}"

echo
echo "making neccesary directories" 
echo "mkdir -p ${MNT}/{boot/grub,dev,proc,run,sys}"
dirs="boot/grub dev etc proc run sys"
for dir in ${dirs}
do
  mkdir -p ${MNT}/${dir}
  exit_err $? "Could not create directory: ${MNT}/${dir}"
done

echo
echo " mount drive {/dev/sda} to /boot/grub"
mount $BOOTDRIVE ${MNT}/boot/grub
exit_err $? "Could not mount boot partition: ${BOOTDRIVE}"

echo
echo "mounting necessary directories"
dirs="dev proc run sys"
for dir in ${dirs}
do
  mount --rbind /${dir} ${MNT}/${dir}
  exit_err $? "Could not mount directory: ${MNT}/${dir}"
done

datasets="home var var/logs var/tmp var/mail"
for ds in ${datasets}
do
echo "Creating Dataset: ${ds}"
  zfs create -o compression=lz4 ${ZPOOL}/${ds}
  exit_err $? "Could not create dataset: ${ZPOOL}/${ds}"
done

echo
echo "Installing MUSL voidlinux, before chroot into it"
XBPS_ARCH=x86_64-musl xbps-install -y -S --repository=${REPO} -r ${MNT} base-system grub ${PACKAGES}
exit_err $? "Could not install void packages!!"

echo
echo "copying a valid resolv.conf into directory, before chroot to get to the new install"
if [ -e "/etc/resolv.conf" ] ; then
  #Copy the current host resolv.conf (assume it is working)
  cp /etc/resolv.conf ${MNT}/etc/resolv.conf
fi

#Now inject a couple always-working DNS nameservers into the end of resolv.conf
echo "8.8.8.8" >> ${MNT}/etc/resolv.conf
echo "8.8.4.4" >> ${MNT}/etc/resolv.conf
#Also copy over the hostid file we had to create manually earlier
cp /etc/hostid ${MNT}/etc/hostid

echo "CHROOT into mount and finish setting up"

#passwd root
echo "KEYMAP=\"us\"" >> ${MNT}/etc/rc.conf
echo "TIMEZONE=\"America/New_York\"" >> ${MNT}/etc/rc.conf
echo "HARDWARECLOCK=\"UTC\"" >> ${MNT}/etc/rc.conf
echo ${HOSTNAME} > ${MNT}/etc/hostname

echo "sync repo, add additional repo, and then re-sync"
${CHROOT} xbps-install -y -S void-repo-nonfree
exit_err $? "Could not install the nonfree repo"
${CHROOT} xbps-install -y -S

echo
echo "NOW install zfs and other packages which make config changes on install"
${CHROOT} xbps-install -y zfs ${PACKAGES_CHROOT}
exit_err $? "Could not install packages: ${PACKAGES_CHROOT}"
echo

echo
echo "making sure we have this file /etc/zfs/zpool.cache" 
${CHROOT} zpool set cachefile=/etc/zfs/zpool.cache ${ZPOOL}
exit_err $? "Could not set cachefile for pool inside chroot"
${CHROOT} zpool set bootfs=${ZPOOL}/ROOT/void ${ZPOOL}
exit_err $? "Could not set bootfs for pool inside chroot"

echo
echo "Auto-enabling services"
for service in  ${SERVICES_ENABLED}
do
  echo " -> ${service}"
  ${CHROOT} ln -s /etc/sv/${service} /var/service/${service}
  exit_err $? "Could not enable service: ${service}"
done

echo
echo "Fix dracut and kernel config, then update grub"
echo "hostonly=\"yes\"" >> ${MNT}/etc/dracut.conf.d/zol.conf
echo "nofsck=\"yes\"" >> ${MNT}/etc/dracut.conf.d/zol.conf
echo "add_dracutmodules+=\" zfs \"" >> ${MNT}/etc/dracut.conf.d/zol.conf
echo "omit_dracutmodules+=\" btrfs resume \"" >> ${MNT}/etc/dracut.conf.d/zol.conf
${CHROOT} xbps-reconfigure -f linux5.2
#Now reinstall grub on the boot device after the reconfiguration
if [ "zfs" != $(${CHROOT} grub-probe /) ] ; then
  echo "ERROR: Could not verify ZFS nature of /"
  exit 1
fi  
#Setup the GRUB configuration
echo '
GRUB_DEFAULT=0
GRUB_TIMEOUT=5
GRUB_DISTRIBUTOR="Void"
GRUB_CMDLINE_LINUX_DEFAULT="loglevel=4 elevator=noop"
GRUB_BACKGROUND=/usr/share/void-artwork/splash.png
GRUB_DISABLE_OS_PROBER=true
' > ${MNT}/etc/default/grub

${CHROOT} grub-install ${BOOTDEVICE}

echo "========="
echo "Final Steps: 1 / 2 - change root password"
echo "========="
passwd -R ${MNT}
echo "========="
echo "Final Steps: 2 / 2 - create user account"
echo "========="
adduser -R ${MNT}

echo "========="
#Now unmount everything and clean up
umount -n ${MNT}/boot/grub
umount -n ${MNT}/dev
umount -n ${MNT}/proc
umount -n ${MNT}/run
umount -n ${MNT}/sys
zpool export ${ZPOOL}

echo "[SUCCESS] Reboot the system and remove the install media to boot into the new system"
