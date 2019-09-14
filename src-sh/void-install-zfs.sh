#!/bin/bash

# This script was influenced by https://wiki.voidlinux.org/Manual_install_with_ZFS_root
SYSTEMDRIVE="/dev/sda2"
BOOTDRIVE="/dev/sda1"
BOOTDEVICE="/dev/sda"
ZPOOL="trident"
MOUNT="/mnt/"
REPO="http://alpha.de.repo.voidlinux.org/current/musl"
PACKAGES=""
#PACKAGES_CHROOT="iwd wpa_supplicant dhcpcd bluez linux-firmware foomatic-db-nonfree vlc phototonic trojita telegram-desktop falkon lynx qterminal openvpn git pianobar w3m ntfs-3g fuse-exfat simple-mtpfs fish-shell zsh x264 libdvdcss gutenprint foomatic-db hplip tor nano xorg lumina dhclient"
PACKAGES_CHROOT="iwd wpa_supplicant dhcpcd bluez linux-firmware falkon lynx qterminal git fish-shell zsh nano xorg-minimal lumina dhclient nano"
SERVICES_ENABLED="dbus sshd dhcpcd dhclient cupsd wpa_supplicant"

## Some important packages
## nano xorg lumina iwd wpa_supplicant dhcpcd bluez linux-firmware-network

if [ ! -e "/bin/zpool" ] ; then
  #Need to install the zfs package first
  xbps-install -S
  xbps-install -y zfs gptdisk mtools
fi

echo "Create the pool"
echo "zpool create -f <pool_name> /dev/sda2"
zpool create -f ${ZPOOL} $SYSTEMDRIVE
echo
echo "Create a fs for the root file systems:"
echo "zfs create  <pool_name>/ROOT"
zfs create  ${ZPOOL}/ROOT
echo
echo "Create a fs for the Void file system"
echo "zfs create <pool_name>/ROOT/<pool_name>"
# zfs create ${ZPOOL}/ROOT/$HOSTNAME
zfs create ${ZPOOL}/ROOT
echo
echo "Unmount all ZFS filesystems:"
echo "zfs umount -a"
zfs umount -a
echo
echo "set mount point"
echo "zfs set mountpoint=/ <pool_name>/ROOT/<pool_name>"
# zfs set mountpoint=legacy ${ZPOOL}/ROOT/$HOSTNAME
zfs set mountpoint=legacy ${ZPOOL}/ROOT
echo
echo "set bootfs"
echo "zpool set bootfs=rpool/ROOT/voidlinux_1 <pool_name>"
# zpool set bootfs=${ZPOOL}/ROOT/$HOSTNAME ${ZPOOL}
zpool set bootfs=${ZPOOL}/ROOT ${ZPOOL}
echo
#echo "Export the pool"
#echo "zpool set bootfs=rpool/ROOT/voidlinux_1 <pool_name>"
#zpool export ${ZPOOL}
#echo
#echo "Import the pool below /mnt:"
#echo "zpool import -R /mnt ${ZPOOL}"
#zpool import -R /mnt ${ZPOOL}
#echo
echo "making neccesary directories" 
echo "mkdir -p /mnt/{boot/grub,dev,proc,run,sys}"
dirs="boot/grub dev etc proc run sys"
for dir in ${dirs}
do
  mkdir -p /mnt/${dir}
done
echo
echo " mount drive {/dev/sda} to /boot/grub"
mount $BOOTDRIVE /mnt/boot/grub
echo
echo "mounting necessary directories"
mount --rbind /dev /mnt/dev
mount --rbind /proc /mnt/proc
mount --rbind /run /mnt/run
mount --rbind /sys /mnt/sys
echo
echo " mount drive {/dev/sda} to /boot/grub"
mount $BOOTDRIVE /mnt/boot/grub
echo
echo "creating /home to snapshot"
zfs create -o compression=lz4 			${ZPOOL}/home
echo
#echo "for things that we probably don't need to clone"
zfs create -o compression=lz4 			${ZPOOL}/var
zfs create -o compression=lz4            		${ZPOOL}/var/logs
zfs create -o compression=lz4          		${ZPOOL}/var/tmp
zfs create -o compression=lz4	          	${ZPOOL}/var/mail
echo
echo "Installing MUSL voidlinux, before chroot into it"
xbps-install -Sy
XBPS_ARCH=x86_64-musl xbps-install -Sy --repository=${REPO}  -r /mnt base-system grub grub-i386-efi grub-x86_64-efi ${PACKAGES}
echo
echo "copying a valid resolv.conf into directory, before chroot to get to the new install"
if [ -e "/etc/resolv.conf" ] ; then
  #Copy the current host resolv.conf (assume it is working)
  cp /etc/resolv.conf /mnt/etc/resolv.conf
  cp /etc/resolv.conf /mnt/root/resolv.conf
fi
#Now inject a couple always-working DNS nameservers into the end of resolv.conf
echo "8.8.8.8" >> /mnt/etc/resolv.conf
echo "8.8.4.4" >> /mnt/etc/resolv.conf

# echo "CHROOT into mount and finish setting up"
#  chroot ${_mnt} grub-install /dev/${_disk}
echo "CHROOT commands to test"

# chroot /mnt/ /bin/bash
echo "setting up /"
	    chroot ${MOUNT} chown root:root /
	    chroot ${MOUNT} chmod 755 /
	    chroot ${MOUNT} passwd root
echo

echo "sync repo, add additional repo, and then re-sync"
	    chroot ${MOUNT} xbps-install -S
	    chroot ${MOUNT} xbps-install -y void-repo-nonfree 
	    chroot ${MOUNT} xbps-install -S

echo
echo "NOW install zfs and other packages which make config changes on install"
	    chroot ${MOUNT} xbps-install zfs -y ${PACKAGES_CHROOT}
echo

echo
echo" Check if grub-probe / outputs zfs, else hack /usr/sbin/grub-mkconfig and insert values of"
echo "GRUB_DEVICE=/dev/sda2 and GRUB_DEVICE_BOOT=/dev/sda2 directly."
echo
# not needed at this point
#	    chroot ${MOUNT} if [ "zfs" != $(grub-probe /) ] ; then
# echo "ERROR: Could not verify ZFS nature of /"
# exit 1
# fi  
echo
# not needed at this point
#	    chroot ${MOUNT} grub-install ${BOOTDEVICE}

echo
echo "making sure we have this file /etc/zfs/zpool.cache" 
	    chroot ${MOUNT} zpool set cachefile=/etc/zfs/zpool.cache ${ZPOOL}
echo

echo
echo "Auto-enabling services"
for service in  ${SERVICES_ENABLED}
do
  echo " -> ${service}"
	    chroot ${MOUNT}  ln -s /etc/sv/${service} /var/service/${service}
done

echo
echo "Fix dracut and kernel config, then update grub"
	    chroot ${MOUNT} echo hostonly=yes >> /etc/dracut.conf
	    chroot ${MOUNT} xbps-reconfigure -f linux5.2
#Now reinstall grub on the boot device after the reconfiguration
#	    chroot ${MOUNT}if [ "zfs" != $(grub-probe /) ] ; then
 # echo "ERROR: Could not verify ZFS nature of /"
 # exit 1
# fi  
 chroot ${MOUNT} grub-probe /

	    chroot ${MOUNT} grub-install ${BOOTDEVICE}

echo "=============="
echo "FINAL STEP: edit /etc/rc.conf to uncomment info as necessary"
echo "1. nano /etc/rc.conf"
echo "2. reboot when ready"
echo "=============="
