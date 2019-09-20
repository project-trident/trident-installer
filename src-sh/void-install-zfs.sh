#!/bin/bash


exit_err(){
  if [ $1 -ne 0 ] ; then
    echo "[ERROR] $2"
    exit 1
  fi
}

if [ ! -e "/bin/zpool" ] ; then
  #Need to install the zfs package first
  echo "[ERROR] The zfs package/tools are not available on this ISO!!"
  exit 1
fi

#Main setting - just pick a disk
DISK=""
clear
echo "================="
echo "Project Trident Installer"
echo "================="
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
BOOTDEVICE="${DISK}"
ZPOOL="trident"
REPO="http://alpha.de.repo.voidlinux.org/current/musl"
PACKAGES=""
INITBE="initial"
#Full package list
#PACKAGES_CHROOT="iwd bluez vlc trojita telegram-desktop falkon qterminal openvpn git pianobar ntfs-3g fuse-exfat simple-mtpfs fish-shell zsh libdvdcss gutenprint foomatic-db foomatic-db-nonfree nano xorg-minimal lumina"
#Minimal package list for testing
PACKAGES_CHROOT="iwd bluez nano xorg-minimal lumina qterminal falkon git"
SERVICES_ENABLED="dbus sshd dhcpcd cupsd wpa_supplicant"
MNT="/run/ovlwork/mnt"
CHROOT="chroot ${MNT}"
## Some important packages
## intel-ucode ?

if [ ! -d "${MNT}" ] ; then
  mkdir -p "${MNT}"
  exit_err $? "Could not create mountpoint directory: ${MNT}"
fi

echo "repository=${REPO}" > /etc/xbps.d/repo.conf
export XBPS_ARCH=x86_64-musl 
#xbps-install -y -S
#exit_err $? "Could not contact package repository!! Fix networking and try again (no changes to disks yet)."

#Check if we are using EFI boot
efibootmgr > /dev/null
if [ $? -eq 0 ] ; then
  #Using EFI
  BOOTMODE="EFI"
else
  BOOTMODE="LEGACY"
fi
echo "Formatting the disk: ${BOOTMODE} ${DISK}"
sfdisk -w always ${DISK} << EOF
	label: gpt
	,100M,U
	,1M,L,*
	;
EOF
exit_err $? "Could not partition the disk: ${DISK}"
EFIDRIVE="${DISK}1"
BOOTDRIVE="${DISK}2"
SYSTEMDRIVE="${DISK}3"

#Formatting the boot partition (FAT32)
mkfs -t msdos ${EFIDRIVE}

# Setup the void tweaks for ZFS 
# Steps found at: https://github.com/nightah/void-install
xbps-reconfigure -a
modprobe zfs
exit_err $? "Could not verify ZFS module"

ip link sh | grep ether | cut -d ' ' -f 6 | tr -d ":" >> /etc/hostid

echo "Creating ZFS Pool: ${ZPOOL}"
zpool create -f -o cachefile=/tmp/zpool.cache -o ashift=12 -d \
		-o feature@async_destroy=enabled \
		-o feature@bookmarks=enabled \
		-o feature@embedded_data=enabled \
		-o feature@empty_bpobj=enabled \
		-o feature@enabled_txg=enabled \
		-o feature@extensible_dataset=enabled \
		-o feature@filesystem_limits=enabled \
		-o feature@hole_birth=enabled \
		-o feature@large_blocks=enabled \
		-o feature@lz4_compress=enabled \
		-o feature@spacemap_histogram=enabled \
		-o feature@userobj_accounting=enabled \
		-O acltype=posixacl \
		-O canmount=off \
		-O compression=lz4 \
		-O devices=off \
		-O mountpoint=none \
		-O normalization=formD \
		-O relatime=on \
		-O xattr=sa \
		${ZPOOL} ${SYSTEMDRIVE}
exit_err $? "Could not create pool: ${ZPOOL} on ${SYSTEMDRIVE}"
#Configure the pool now
zfs set compression=on ${ZPOOL}
zfs create -o canmount=off ${ZPOOL}/ROOT
zfs create -o mountpoint=/ -o canmount=noauto ${ZPOOL}/ROOT/${INITBE}
exit_err $? "Could not create ROOT dataset"

zpool set bootfs=${ZPOOL}/ROOT/${INITBE} ${ZPOOL}
exit_err $? "Could not set ROOT/${INITBE} dataset as bootfs"

echo "Verify pool can be exported/imported"
zpool export ${ZPOOL}
exit_err $? "Could not export pool"
zpool import -R ${MNT} ${ZPOOL}
exit_err $? "Could not import the new pool at ${MNT}"
#need to manually mount the root dataset (noauto)
zfs mount ${ZPOOL}/ROOT/${INITBE}
exit_err $? "Count not mount the root ZFS dataset"

datasets="home var var/logs var/tmp var/mail"
for ds in ${datasets}
do
echo "Creating Dataset: ${ds}"
  zfs create -o compression=lz4 -o mountpoint=/${ds} ${ZPOOL}/${ds}
  exit_err $? "Could not create dataset: ${ZPOOL}/${ds}"
done

dirs="boot/grub boot/EFI dev etc proc run sys"
for dir in ${dirs}
do
  mkdir -p ${MNT}/${dir}
  exit_err $? "Could not create directory: ${MNT}/${dir}"
done

mount $EFIDRIVE ${MNT}/boot/EFI
exit_err $? "Could not mount EFI boot partition: ${EFIDRIVE} -> ${MNT}/boot/EFI (${BOOTMODE})"


dirs="dev proc sys"
for dir in ${dirs}
do
  mount --rbind /${dir} ${MNT}/${dir}
  exit_err $? "Could not mount directory: ${MNT}/${dir}"
done

echo
echo "Installing MUSL voidlinux, before chroot into it"
xbps-install -y -S --repository=${REPO} -r ${MNT} base-system grub ${PACKAGES}
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
cp /etc/xbps.d/repo.conf ${MNT}/etc/xbps.d/repo.conf

echo "CHROOT into mount and finish setting up"

echo "KEYMAP=\"us\"" >> ${MNT}/etc/rc.conf
echo "TIMEZONE=\"America/New_York\"" >> ${MNT}/etc/rc.conf
echo "HARDWARECLOCK=\"UTC\"" >> ${MNT}/etc/rc.conf
echo ${HOSTNAME} > ${MNT}/etc/hostname

echo "Setting up repositories"
${CHROOT} xbps-install -y -S void-repo-nonfree
exit_err $? "Could not install the nonfree repo"
${CHROOT} xbps-install -y -S

echo
echo "Fix dracut and kernel config, then update grub"
echo "hostonly=\"yes\"" >> ${MNT}/etc/dracut.conf.d/zol.conf
echo "nofsck=\"yes\"" >> ${MNT}/etc/dracut.conf.d/zol.conf
echo "add_dracutmodules+=\" zfs \"" >> ${MNT}/etc/dracut.conf.d/zol.conf
echo "omit_dracutmodules+=\" btrfs resume \"" >> ${MNT}/etc/dracut.conf.d/zol.conf
${CHROOT} xbps-reconfigure -f linux5.2

echo
echo "Installing packages within chroot"
mkdir ${MNT}/tmp/pkg-cache
rm ${MNT}/var/cache/xbps/*
for pkg in zfs ${PACKAGES_CHROOT}
do
  echo
  echo "Installing package: ${pkg}"
  ${CHROOT} xbps-install -y -c /tmp/pkg-cache ${pkg}
  exit_err $? "Could not install package: ${pkg}"
  rm ${MNT}/tmp/pkg-cache/*
done
echo
#Now remove the temporary pkg cache directory in the chroot
rm -r ${MNT}/tmp/pkg-cache

echo
echo "Auto-enabling services"
for service in  ${SERVICES_ENABLED}
do
  echo " -> ${service}"
  ${CHROOT} ln -s /etc/sv/${service} /var/service/${service}
  exit_err $? "Could not enable service: ${service}"
done

#Now reinstall grub on the boot device after the reconfiguration
if [ "zfs" != $(${CHROOT} grub-probe /) ] ; then
  echo "ERROR: Could not verify ZFS nature of /"
  exit 1
fi  
#Setup the GRUB configuration
echo "
GRUB_DEFAULT=0
GRUB_TIMEOUT=5
GRUB_DISTRIBUTOR=\"Project Trident\"
GRUB_CMDLINE_LINUX_DEFAULT=\"loglevel=4 elevator=noop\"
GRUB_BACKGROUND=/usr/share/void-artwork/splash.png
GRUB_CMDLINE_LINUX=\"root=ZFS=${ZPOOL}/ROOT/${INITBE}\"
GRUB_DISABLE_OS_PROBER=true
" > ${MNT}/etc/default/grub

#Stamp GPT loader on disk itself
${CHROOT} grub-install ${BOOTDEVICE}
#Stamp EFI loader on the EFI partition
${CHROOT} grub-install --target=x86_64-efi --efi-directory=/boot/EFI --bootloader-id=debian --recheck --no-floppy

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
umount -n ${MNT}/boot/EFI
umount -n ${MNT}/dev
umount -n ${MNT}/proc
umount -n ${MNT}/sys
zpool export ${ZPOOL}

echo "[SUCCESS] Reboot the system and remove the install media to boot into the new system"
