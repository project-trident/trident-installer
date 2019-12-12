#!/bin/bash

SERVER_PACKAGES="iwd nano git jq fzf kexec-tools zsh fish-shell wireguard"
LITE_PACKAGES="${SERVER_PACKAGES} bluez noto-fonts-ttf xorg-minimal lumina qterminal compton hicolor-icon-theme xrandr qt5-svg wpa-cute libdvdcss gutenprint ntfs-3g fuse-exfat simple-mtpfs"
FULL_PACKAGES="${LITE_PACKAGES} telegram-desktop vlc firefox trojita pianobar"

if [ "${1}" = "-h" ] || [ "${1}" = "help" ] || [ "${1}" = "--help" ] ; then
echo "Project Trident Installer
--------------------------
This install script is interactive by default, but can be made non-interactive by setting various environment variables before launching the script.

Any variable that is marked with a Y will prompt for a value if not set beforehand.

Prompt : Variable : Example Value      : Explanation
---------------------------------
 N     : TITLE     : Trident Installer  : Title for interactive prompt dialogs
 Y     : DISK      : /dev/sda           : Which disk will be installed to
 Y     : REPOTYPE  : glibc *or* musl    : Repository type
 Y     : SWAPSIZE  : 3G                 : swap partition size. 0 to disable
 Y     : NHOSTNAME : Trident-XXXX       : New system hostname
 Y     : ZPOOL     : trident            : ZFS pool name to create
 N     : INITBE    : initial            : Name of the initial boot environment
 Y     : KEYMAP    : us                 : Keyboard layout to use after install
 N     : TIMEZONE  : America/New_York   : Timezone to use after install
 Y     : ROOTPW    : myrootpw           : Password to set for the root account
 Y     : PACKAGES : bluez lumina      : Space-delimited list of packages to install
"
  exit 0
fi

LOGFILE="${1}"

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

#Global title for dialog
if [ -z "${TITLE}" ] ; then
  TITLE="Project Trident Installer"
fi

get_dlg_ans(){
  # INPUTS:
  #   TITLE: Title to use for the dialog
  #   CLI Args : Arguments for dialog (option_name, option_text, repeat...)
  # OUTPUTS:
  #   ANS: option_name selected by user

  local TANS="/tmp/.dlg.ans.$$"
  if [ -e "$TANS" ] ; then rm ${TANS}; fi
  if [ -e "$TANS.dlg" ] ; then rm ${TANS}.dlg; fi
  while :
  do
    echo "dialog --title \"$TITLE\" ${@}" >${TANS}.dlg
    sh ${TANS}.dlg 2>${TANS}
    local _ret=$?
    if [ $_ret -ne 0 ] || [ ! -e "$TANS" ] ; then
      #echo "Cancel detected : ${CURMENU} ${_ret}"
      #sleep 1
      rm ${TANS} 2>/dev/null
    fi

    if [ ! -e "$TANS" ] ; then
       ANS=""
    else
      ANS=`cat ${TANS}`
      rm ${TANS}
    fi
    #echo "Got Ans: ${ANS}"
    #sleep 2
    rm ${TANS}.dlg
    return ${_ret}
  done
}

getDisks(){
  #generate the disk list
  opts=""
  opts=$(sfdisk -l | grep "Disk /dev/" | grep -v "/loop" | cut -d , -f 1 | cut -d / -f 3- | while read _disk
  do
    echo -n " $(echo $_disk | cut -d : -f 1) \"$(echo $_disk | cut -d ' ' -f 2-)\""
  done )
  get_dlg_ans "--menu \"Which disk do you want to install to?\" 0 0 0 . \"Rescan for devices\" ${opts}"
  if [ "${ANS}" = "." ] ; then
    ANS=""
  elif [ -z "${ANS}" ] ; then
    exit 1 #cancelled
  fi
  export DISK="/dev/${ANS}"
}

getRepotype(){
  opts=" glibc \"Standard packages (default)\" musl \"Lightweight system. No proprietary packages\" "
  get_dlg_ans "--menu \"Pick the system package type. This cannot be easily changed later.\" 0 0 0 ${opts}"
  export REPOTYPE="${ANS}"
}

getSwap(){
  opts=" none \"No swap space\" 1G \"\" 2G \"\" 4G \"Typical size\" 8G \"\""
  get_dlg_ans "--menu \"Select the encrypted SWAP size.\" 0 0 0 ${opts}"
  if [ "$ANS" = "none" ] ; then
    ANS=""
  fi
  export SWAPSIZE="${ANS}"
}

getPassword(){
  TMP="1"
  TMP2="2"
  while [ "${TMP}" != "${TMP2}" ]
  do
    get_dlg_ans "--passwordbox \"Enter password for ${1}\n(Note: Hidden Text)\" 0 0"
    TMP="${ANS}"
    get_dlg_ans "--passwordbox \"Repeat password for ${1}\" 0 0"
    TMP2="${ANS}"
  done
  ANS="${TMP}"
  unset TMP
  unset TMP2
}

adjustTextValue(){
  # Input 1 : box text
  # Input 2 : current value
  get_dlg_ans "--inputbox \"${1}\" 0 0 \"${2}\""
  if [ -z "${ANS}" ] ; then
    ANS="${2}" #reset back to initial default value
  fi
}

getPackages(){
opts="Full \"[TO-DO] Desktop install with lots of extra tools\" Lite \"[TO-DO] Desktop install with no extra tools\" Server \"CLI install for hobbyists\" Void \"ZFS install of Void Linux\" "
  get_dlg_ans "--menu \"Select the package set to install. Packages are easily changed later.\\n\\n[WARNING] The Full and Lite desktop options are still a work in progress and not fully-implemented.\" 0 0 0 ${opts}"
  case ${ANS} in
    Full)
	PACKAGES_CHROOT="${FULL_PACKAGES}"
	;;
    Lite)
	PACKAGES_CHROOT="${LITE_PACKAGES}"
	;;
    Server)
	PACKAGES_CHROOT="${SERVER_PACKAGES}"
	;;
    default)
	PACKAGES_CHROOT=""
	;;
  esac
  export PACKAGES_CHROOT
}

cleanupInstall(){
  #Now unmount everything and clean up
  umount -nfR ${MNT}/boot/efi
  umount -nfR ${MNT}/dev
  umount -nfR ${MNT}/proc
  umount -nfR ${MNT}/sys
  umount -nfR ${MNT}/var
  umount -nfR ${MNT}
  zpool export ${ZPOOL}
}

doInstall(){
# Install function. Nothing interactive should ever be in here
# typically piped through tee to log the output

if [ ! -d "${MNT}" ] ; then
  mkdir -p "${MNT}"
  exit_err $? "Could not create mountpoint directory: ${MNT}"
fi

echo "Starting Installation...

"
echo "-----------------"
echo "Step 1 : Formatting the disk"
echo "-----------------"
echo "Erasing the first 200MB of the disk"
dd if=/dev/zero of=${DISK} bs=100M count=2

#xbps-install -y -S --repository=${REPO}
#echo "repository=${REPO}" > /etc/xbps.d/repo.conf

echo "Formatting the disk: ${BOOTMODE} ${DISK}"
sfdisk -w always ${DISK} << EOF
	label: gpt
	,200M,U
	,10M,21686148-6449-6E6F-744E-656564454649,*
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
zpool create -f -o ashift=12 -d \
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
#Add a custom dataset for docker (that service can do bad things to the /var dataset)
echo "Creating Dataset: var/docker"
zfs create -o compression=lz4 -o mountpoint=/var/lib/docker ${ZPOOL}/var/docker
exit_err $? "Could not create dataset: ${ZPOOL}/var/docker"

dirs="boot/grub boot/efi dev etc proc run sys"
for dir in ${dirs}
do
  mkdir -p ${MNT}/${dir}
  exit_err $? "Could not create directory: ${MNT}/${dir}"
done

mount $EFIDRIVE ${MNT}/boot/efi
exit_err $? "Could not mount EFI boot partition: ${EFIDRIVE} -> ${MNT}/boot/efi (${BOOTMODE})"


dirs="dev proc sys run"
for dir in ${dirs}
do
  mount --rbind /${dir} ${MNT}/${dir}
  exit_err $? "Could not mount directory: ${MNT}/${dir}"
done


echo
echo "-------------------------------"
echo "Step 2: Installing base system"
echo "-------------------------------"
#NOTE: Do NOT install the ZFS package yet - that needs to run inside chroot for post-install actions.
xbps-install -y -S -r "${MNT}" --repository="${REPO}"
xbps-install -y -r "${MNT}" --repository="${REPO}" base-system grub grub-i386-efi grub-x86_64-efi ${PACKAGES}
exit_err $? "Could not install void packages!!"

linuxver=`${CHROOT} xbps-query linux | grep pkgver | cut -d - -f 2 | cut -d . -f 1-2 | cut -d _ -f 1`
echo "Got Linux Version: ${linuxver}"

echo
echo "copying a valid resolv.conf into directory, before chroot to get to the new install"
if [ -e "/etc/resolv.conf" ] ; then
  #Copy the current host resolv.conf (assume it is working)
  cp /etc/resolv.conf ${MNT}/etc/resolv.conf
fi
#Copy over any saved wifi networks from the ISO
cp "/etc/wpa_supplicant/wpa_supplicant.conf" "${MNT}/etc/wpa_supplicant/wpa_supplicant.conf"

#Now inject a couple always-working DNS nameservers into the end of resolv.conf
echo "8.8.8.8" >> ${MNT}/etc/resolv.conf
echo "8.8.4.4" >> ${MNT}/etc/resolv.conf
#Also copy over the hostid file we had to create manually earlier
cp /etc/hostid ${MNT}/etc/hostid
#cp /etc/xbps.d/repo.conf ${MNT}/etc/xbps.d/repo.conf

echo "KEYMAP=\"${KEYMAP}\"" >> ${MNT}/etc/rc.conf
echo "TIMEZONE=\"${TIMEZONE}\"" >> ${MNT}/etc/rc.conf
echo "HARDWARECLOCK=\"UTC\"" >> ${MNT}/etc/rc.conf
echo ${NHOSTNAME} > ${MNT}/etc/hostname

#ensure passwords are encrypted by the most-secure algorithm available by default
echo "ENCRYPT_METHOD    SHA512" >> ${MNT}/etc/login.defs

#Change the root password
${CHROOT} echo "root:${ROOTPW}" |  ${CHROOT} chpasswd -c SHA512
exit_err $? "Could not set root password"

echo "Setting up repositories"
${CHROOT} xbps-install -y -S
${CHROOT} xbps-install -y void-repo-nonfree
exit_err $? "Could not install the nonfree repo"
${CHROOT} xbps-install -y -S

echo
echo "Fix dracut and kernel config, then update grub"
echo "hostonly=\"yes\"" >> ${MNT}/etc/dracut.conf.d/zol.conf
echo "nofsck=\"yes\"" >> ${MNT}/etc/dracut.conf.d/zol.conf
echo "add_dracutmodules+=\"zfs btrfs resume\"" >> ${MNT}/etc/dracut.conf.d/zol.conf
${CHROOT} xbps-reconfigure -f linux${linuxver}

echo
echo "Installing packages within chroot"
mkdir ${MNT}/tmp/pkg-cache
rm ${MNT}/var/cache/xbps/*
# Required packages
for pkg in zfs cryptsetup ${PACKAGES_CHROOT}
do
  echo
  echo "Installing package: ${pkg}"
  ${CHROOT} xbps-install -y -c /tmp/pkg-cache ${pkg}
  if [ $? -ne 0 ] ; then
    echo "[WARNING] Error installing package: ${pkg}"
    echo " - Retrying in 2 seconds"
    sleep 2
    ${CHROOT} xbps-install -y -c /tmp/pkg-cache ${pkg}
  fi
  exit_err $? "Could not install package: ${pkg}"
  rm ${MNT}/tmp/pkg-cache/*
done

#Now remove the temporary pkg cache directory in the chroot
echo
rm -r ${MNT}/tmp/pkg-cache

# Now setup SWAP on the device
if [ -n "${SWAPSIZE}" ] && [ 0 != "${SWAPSIZE}" ] ; then
  echo "Setting up encrypted SWAP on the device: ${SWAPSIZE}"
  ${CHROOT} zfs create -V ${SWAPSIZE} -b $(getconf PAGESIZE) -o compression=zle \
      -o logbias=throughput -o sync=always \
      -o primarycache=metadata -o secondarycache=none \
      -o com.sun:auto-snapshot=false ${ZPOOL}/swap
  if [ $? -eq 0 ] ; then
    echo "swap  /dev/zvol/${ZPOOL}/swap  /dev/urandom  swap,cipher=aes-cbc-essiv:sha256,size=256" >> ${MNT}/etc/crypttab
    echo "/dev/mapper/swap none swap defaults 0 0" >> ${MNT}/etc/fstab
  else
    echo "[ERROR] Swap could not get setup properly - this will need to be done by hand later"
    zfs destroy ${ZPOOL}/swap
    sleep 2 #allow the user to see this error message before continuing
  fi
fi

echo
echo "Auto-enabling services"
for service in ${SERVICES_ENABLED}
do
  if [ ! -e "/etc/sv/${service}" ] ; then continue ; fi
  echo " -> ${service}"
  ${CHROOT} ln -s /etc/sv/${service} /var/service/${service}
  exit_err $? "Could not enable service: ${service}"
done

#Now reinstall grub on the boot device after the reconfiguration
if [ "zfs" != $(${CHROOT} grub-probe /) ] ; then
  echo "ERROR: Could not verify ZFS nature of /"
  exit 1
fi
#Chase down a fix that was also added to the grub auto-scripts
export ZPOOL_VDEV_NAME_PATH=YES
#Get the disk UUID for the boot disk
diskuuid=$(blkid --output export ${SYSTEMDRIVE} | grep -E '^(UUID=)' | cut -d = -f 2  )
echo "Got ZFS pool disk uuid: ${diskuuid}"
#Setup the GRUB configuration
mkdir -p ${MNT}/etc/defaults
wallpaper=$(ls /root/Trident-wallpaper.*)
wallpaper=$(basename ${wallpaper})
wallfmt=$(echo ${wallpaper} | cut -d . -f 2)
cp "/root/${wallpaper}" "${MNT}/etc/defaults/grub-splash.${wallfmt}"
echo "
GRUB_DEFAULT=0
GRUB_TIMEOUT=5
GRUB_DISTRIBUTOR=\"Project-Trident\"
GRUB_CMDLINE_LINUX_DEFAULT=\"loglevel=4 elevator=noop\"
GRUB_BACKGROUND=/etc/defaults/grub-splash.${wallfmt}
GRUB_CMDLINE_LINUX=\"root=ZFS=${ZPOOL}/ROOT/${INITBE}\"
GRUB_DISABLE_OS_PROBER=false
" > ${MNT}/etc/default/grub
#GRUB_CMDLINE_LINUX=\"root=LABEL=${ZPOOL}\" #Does not know it is ZFS and throws a fit
#GRUB_CMDLINE_LINUX=\"root=UUID=${diskuuid}\"

# to see if these help
# grub needs updating after we make changes
#echo "updating grub"
#${CHROOT} update-grub
${CHROOT} zpool set cachefile=/etc/zfs/zpool.cache "${ZPOOL}"
${CHROOT} xbps-reconfigure -f linux${linuxver}
#${CHROOT} lsinitrd -m

echo "Installing GRUB bootloader"
#Stamp GPT loader on disk itself
#${CHROOT} grub-mkconfig -o {MNT}/boot/grub/grub.cfg
${CHROOT} grub-install ${BOOTDEVICE}
#Stamp EFI loader on the EFI partition
#Create a project-trident directory only
${CHROOT} grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=Project-Trident --recheck --no-floppy
mkdir -p "${MNT}/boot/efi/EFI/boot/"
#Copy the EFI registration to the default boot path as well
cp "${MNT}/boot/efi/EFI/project-trident/grubx64.efi" "${MNT}/boot/efi/EFI/boot/bootx64.efi"

echo
echo "[SUCCESS] Reboot the system and remove the install media to boot into the new system"

} #end of the doInstall function

# ===============
#  LOAD SETTINGS
# ===============
while [ -z "${DISK}" ]
do
  getDisks
done
if [ -z "${SWAPSIZE}" ] ; then
  getSwap
fi
if [ -z "${REPOTYPE}" ] ; then
  getRepotype
fi
if [ -z "${ROOTPW}" ] ; then
  getPassword "root"
  ROOTPW="${ANS}"
  unset ANS
fi
if [ -z "${NHOSTNAME}" ] ; then
  NHOSTNAME="Trident-${RANDOM}"  
  adjustTextValue "Select system hostname" "${NHOSTNAME}"
  NHOSTNAME="${ANS}"
fi
if [ -z "${ZPOOL}" ] ; then
  zpool import -aN
  ANS=""
  while [ $? -eq 0 ] || [ -z "${ANS}" ]
  do
    if [ -n "${ANS}" ] ; then
      adjustTextValue "Pool already exists: Select different ZFS pool name" "${ANS}"
    else
      adjustTextValue "Select ZFS pool name" "trident"
    fi
    ZPOOL="${ANS}"
    zpool list "${ANS}"  > /dev/null 2> /dev/null
  done
  # Now unmount/export all zfs pools
  for pool in `zpool list -H | cut -d ' ' -f 1`
  do 
    zpool export "${pool}"
  done
fi
if [ -z "${INITBE}" ] ; then
  INITBE="initial"
fi
if [ -z "${KEYMAP}" ] || [ "${REPOTYPE}" = "musl" ] ; then
  #Localized keyboard maps not supported by musl packages
  KEYMAP="us"
fi
if [ -n "${PACKAGES}" ] ; then
  PACKAGES_CHROOT="${PACKAGES}"
else
  getPackages
fi
if [ -z "${TIMEZONE}" ] ; then
  TIMEZONE="America/New_York"
fi



#Full package list
#PACKAGES_CHROOT="iwd bluez vlc trojita telegram-desktop falkon qterminal openvpn git pianobar ntfs-3g fuse-exfat simple-mtpfs fish-shell zsh libdvdcss gutenprint foomatic-db foomatic-db-nonfree nano xorg-minimal lumina wpa-cute zfz kexec-tools"
#Minimal package list for testing
#PACKAGES=""
#PACKAGES_CHROOT="iwd bluez nano git noto-fonts-ttf jq"
#PACKAGES_OPTIONAL="xorg-minimal lumina qterminal compton hicolor-icon-theme xrandr qt5-svg wpa-cute"
SERVICES_ENABLED="dbus dhcpcd cupsd wpa_supplicant bluetoothd"

# ==============================
#  Generate Internal Variables from settings
# ==============================
BOOTDEVICE="${DISK}"
MNT="/run/ovlwork/mnt"
CHROOT="chroot ${MNT}"

# Automatically adjust the musl/glibc repo switch as needed
if [ "${REPOTYPE}" = "musl" ] ; then
  export XBPS_ARCH=x86_64-musl
  REPO="https://alpha.de.repo.voidlinux.org/current/musl"
else
  export XBPS_ARCH=x86_64
  REPO="https://alpha.de.repo.voidlinux.org/current"
fi
# DO NOT Set target arch. This disables a lot of the post-install configuration for packages
#export XBPS_TARGET_ARCH="${XBPS_ARCH}" 

#Check if we are using EFI boot
efibootmgr > /dev/null 2>/dev/null
if [ $? -eq 0 ] ; then
  #Using EFI
  BOOTMODE="EFI"
else
  BOOTMODE="LEGACY"
fi

if [ -n "${LOGFILE}" ] ; then
  # Split between log and stdout
  doInstall 2>&1 | tee "${LOGFILE}"
else
  # Just use stdout
  doInstall
fi
#Now cleanup before exit
cleanupInstall
