#!/bin/bash


SERVER_PACKAGES="zsh fish-shell firejail openvpn neofetch sl wget trident-core"
LITE_PACKAGES="${SERVER_PACKAGES} wpa-cute ntfs-3g fuse-exfat simple-mtpfs trident-desktop setxkbmap"
FULL_PACKAGES="${LITE_PACKAGES} telegram-desktop vlc firefox trojita pianobar libreoffice cups foomatic-db foomatic-db-engine cups-filters"

SERVICES_ENABLED="dbus dhcpcd cupsd wpa_supplicant bluetoothd acpid nftables dcron autofs openntpd sddm"

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
ORIGTITLE="${TITLE}" #save this for later

get_dlg_ans(){
  # INPUTS:
  #   TITLE: Title to use for the dialog
  #   CLI Args : Arguments for dialog (option_name, option_text, repeat...)
  # OUTPUTS:
  #   ANS: option_name selected by user
  if [ -n "${PAGENUM}" ] ; then
    TITLE="${ORIGTITLE} (${PAGENUM})"
  else
    TITLE="${ORIGTITLE}"
  fi
  local TANS="/tmp/.dlg.ans.$$"
  if [ -e "$TANS" ] ; then rm ${TANS}; fi
  if [ -e "$TANS.dlg" ] ; then rm ${TANS}.dlg; fi
  while :
  do
    echo "dialog --no-cancel --title \"$TITLE\" ${@}" >${TANS}.dlg
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

checkPackages(){
  return #Disable - still not working right yet
  #Reads in the list of PACKAGES_CHROOT and verifies they exist in the repo
  # Missing packages are put into the PACKAGES_MISSING variable
  echo "Verifying packages are in the repository..."
  xbps-install -y -S --repository="${REPO}"
  local okpkgs
  for pkg in ${PACKAGES_CHROOT}
  do
    xbps-query -Ri --repository="${REPO}" -s "^(${pkg}-)[0-9]" --regex 1>/dev/null
    if [ $? -eq 0 ] ; then
      okpkgs="${okpkgs} ${pkg}"
    else
      echo "[WARNING] Package not found in repo: ${pkg}"
      PACKAGES_MISSING="${PACKAGES_MISSING} ${pkg}"
    fi
  done
  PACKAGES_CHROOT="${okpkgs}"
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
  opts=" default \"4G - Average laptop\" none \"No swap space\" 1G \"\" 2G \"\" 4G \"\" 8G \"\" 16G \"\" 32G \"\""
  get_dlg_ans "--menu \"Select the encrypted SWAP size.\" 0 0 0 ${opts}"
  if [ "$ANS" = "none" ] ; then
    ANS=""
  elif [ "${ANS}" = "default" ] ; then
    ANS="4G"
  fi
  export SWAPSIZE="${ANS}"
}

getPassword(){
  TMP="1"
  TMP2="2"
  local minlength=4
  if [ "${1}" != "root" ] ; then minlength=8; fi
  while [ "${TMP}" != "${TMP2}" ]
  do
    get_dlg_ans "--passwordbox \"Enter password for ${1}\n\n(Note: Hidden Text, ${minlength} characters minimum, no spaces or tabs)\" 0 0"
    TMP=$(echo "${ANS}" | tr -d '[:space:]')
    if [ ${minlength} -gt ${#TMP} ] ; then
      get_dlg_ans "--msgbox \"ERROR: Invalid password\" 0 0"
      TMP="1" ; TMP2="2"
    else
      get_dlg_ans "--passwordbox \"Repeat password for ${1}\" 0 0"
      TMP2="${ANS}"
    fi
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

generateHostid(){
# chars must be 0-9, a-f, A-F and exactly 8 chars
local host_id=$(cat /dev/urandom | tr -dc 'a-f0-9' | fold -w 8 | head -n 1)
echo "Auto-generated HostID: ${host_id}"
#Found this snippet below from a random script online - but seems to work fine (Ken Moore: 12/29/19)
a=${host_id:6:2}
b=${host_id:4:2}
c=${host_id:2:2}
d=${host_id:0:2}
echo -ne \\x$a\\x$b\\x$c\\x$d > /etc/hostid
}

getPackages(){
opts="Full \"Desktop install with many extra utilities\" Lite \"Desktop install only\" Server \"Base system setup only\" Void \"Bare-bones install on ZFS\" "
  get_dlg_ans "--menu \"Select the package set to install. Packages are easily changed later.\" 0 0 0 ${opts}"
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

getUser(){
  #  user_crypt : true/false
  #  user : <username>
  #  userpass : <password>
  #  usershell : /bin/bash or other
  #  usercomment : Comment
  user_crypt="false"
  if [ "${BOOTMODE}" = "EFI" ] ; then
    user_crypt="true"
  fi
  while [ -z "${usercomment}" ]
  do
    adjustTextValue "Full name for the user"
    usercomment="${ANS}"
  done
  while [ -z "${user}" ]
  do
    adjustTextValue "Enter the shortened username\n(lowercase, no spaces)" $(echo "${usercomment}" | tr -d '[:space:]' | tr '[:upper:]' '[:lower:]')
    user=$(echo "${ANS}" | tr -d '[:space:]' | tr '[:upper:]' '[:lower:]')
    if [ "${user}" = "root" ] || [ "${user}" = "toor" ] ; then user="" ; fi #do not allow overwrite root account
  done

  getPassword "${user}"
  if [ -n "${ANS}" ] ; then
    userpass="${ANS}"
  fi
  #Add prompt for desired shell later
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

installZfsBootMenu(){
  echo "Installing zfsbootmenu"
  if [ ! -e "${MNT}/etc/zfsbootmenu/config.ini" ] ; then
    # Install the zfsbootmenu custom package if it exists
    pkgfile=$(ls /root/zfsbootmenu*)
    if [ ! -f "${pkgfile}" ] ; then return ; fi
    if [ -f "/root/xdowngrade-quiet" ] ; then
      cp /root/xdowngrade-quiet ${MNT}/usr/bin/xdowngrade
    else
      ${CHROOT} xbps-install -y xtools
    fi
    exit_err $? "Could not install package utilities!" 
    ${CHROOT} xbps-install -y fzf kexec-tools perl-Config-IniFiles
    exit_err $? "Could not install bootloader utilities!"
    cp "${pkgfile}" "${MNT}${pkgfile}"
    ${CHROOT} xdowngrade ${pkgfile}
    exit_err $? "Could not install zfsbootmenu!"
    if [ -f "/root/xdowngrade-quiet" ] ; then
      #Remove temporary xdowngrade script
      rm ${MNT}/usr/bin/xdowngrade
    fi
  fi
  # Setup the config file within the chroot
  sed -i 's|/void|/project-trident|g' "${MNT}/etc/zfsbootmenu/config.ini"
  sed -i 's|ManageImages=0|ManageImages=1|' "${MNT}/etc/zfsbootmenu/config.ini"
  # Now install zfsbootmenu boot entries
  mkdir -p "${MNT}/boot/efi/EFI/project-trident"
  ${CHROOT} xbps-reconfigure -f zfsbootmenu
  # Setup rEFInd
  echo '"Standard quiet boot"  "ro quiet loglevel=0 elevator=noop root="
"Standard boot" "ro loglevel=4 elevator=noop root="
"Single user boot" "ro loglevel=4 elevator=noop single root="
"Single user verbose boot" "ro loglevel=6 elevator=noop single root="
' > "${MNT}/boot/efi/EFI/project-trident/refind_linux.conf"
  #${CHROOT} xbps-reconfigure -f refind
  ${CHROOT} refind-install --usedefault "${EFIDRIVE}"
  exit_err $? "Could not install refind!"
  # Now remove the grub EFI entry that zfsbootmenu generated (if any)
  #rm ${MNT}/boot/efi/EFI/project-trident/*.efi
  # Tweak the rEFInd configuration
  bootsplash=$(ls /root/trident-wallpaper-refind*)
  cp "${bootsplash}" ${MNT}/boot/efi/EFI/boot/.
  echo "# Project Trident branding options" >> "${MNT}/boot/efi/EFI/boot/refind.conf"
  echo "timeout 5" >> "${MNT}/boot/efi/EFI/boot/refind.conf"
  echo "banner $(basename ${bootsplash})" >> "${MNT}/boot/efi/EFI/boot/refind.conf"
  echo "banner_scale fillscreen" >> "${MNT}/boot/efi/EFI/boot/refind.conf"
  #Ensure refind is setup to boot next (even if they don't eject the ISO)
  bootnext=$(efibootmgr | grep "EFI Hard Drive" | cut -d '*' -f 1 | rev | cut -d '0' -f 1)
  efibootmgr -n "${bootnext}"
  # Cleanup the static package file
  rm "${MNT}${pkgfile}"
}

setupPamCrypt(){
  grep -q "pam_zfscrypt.so" "${MNT}/etc/pam.d/passwd"
  if [ $? -ne 0 ] ; then
    echo "password  optional  pam_zfscrypt.so" >> "${MNT}/etc/pam.d/passwd"
  fi
  grep -q "pam_zfscrypt.so" "${MNT}/etc/pam.d/system-auth"
  if [ $? -ne 0 ] ; then
    echo "auth  optional  pam_zfscrypt.so" >> "${MNT}/etc/pam.d/system-auth"
    echo "session  optional  pam_zfscrypt.so  runtime_dir=/tmp/zfscrypt" >> "${MNT}/etc/pam.d/system-auth"
  fi
}

createUser(){
  # Required Inputs:
  #  user_crypt : true/false
  #  user : <username>
  #  userpass : <password>
  #  usershell : /bin/bash or other
  #  usercomment : Comment
  if [ -z "${user}" ] || [ -z "${userpass}" ] ; then
    #No user to be created
    return 0
  fi
  if [ -z "${usershell}" ] ; then
    usershell="/bin/bash"
  fi
  echo "Creating user account: ${user} : ${usershell}"
  #Create the dataset    
  if [ "${user_crypt}" = "true" ] ; then
    # NOTE: encrypted homedirs cannot be used when booting with GRUB
    #  GRUB refuses to recognize ZFS boot pools if *any* datasets are encrypted.
    #Ensure minimum passphrase length is met (8 characters)
    if [ ${#userpass} -lt 8 ] ; then
      echo "[ERROR] Passphrase for ${user} must be 8+ characters when encryption is enabled"
      return 1
    fi
    tmpfile=$(mktemp /tmp/.XXXXXX)
    echo "${userpass}" > "${tmpfile}"
    zfs create -o "mountpoint=/usr/home/${user}" -o "io.github.benkerry:zfscrypt_user=${user}" -o "setuid=off" -o "compression=on" -o "atime=off" -o "encryption=on" -o "keyformat=passphrase" -o "keylocation=file://${tmpfile}" -o "canmount=noauto" "${ZPOOL}/home/${user}"
    if [ $? -eq 0 ] ; then
      zfs mount "${ZPOOL}/home/${user}"
      zfs set "keylocation=prompt" "${ZPOOL}/home/${user}"
    fi
    rm "${tmpfile}"
  else
    zfs create -o "mountpoint=/usr/home/${user}" -o "setuid=off" -o "compression=on" -o "atime=off" -o "canmount=on" "${ZPOOL}/home/${user}"
  fi
  if [ $? -ne 0 ] ; then
    return 1
  fi
  # Create the user
  ${CHROOT} useradd -M -s "${usershell}" -d "/usr/home/${user}" -c "${usercomment}" -G "wheel,users,audio,video,input,cdrom,bluetooth" "${user}"
  if [ $? -ne 0 ] ; then
    return 1
  fi
  ${CHROOT} echo "${user}:${userpass}" |  ${CHROOT} chpasswd -c SHA512
  # Setup ownership of the dataset
  ${CHROOT} chown "${user}:${user}" "/usr/home/${user}"
  # Allow the user to create/destroy child datasets and snapshots on their home dir
  if [ "${user_crypt}" = "true" ] ; then
    ${CHROOT} zfs allow "${user}" load-key,mount,create,destroy,rollback,snapshot "${ZPOOL}/home/${user}"
    zfs unmount "${ZPOOL}/home/${user}"
  else
    ${CHROOT} zfs allow "${user}" mount,create,destroy,rollback,snapshot "${ZPOOL}/home/${user}"
  fi
  if [ $? -ne 0 ] ; then
    return 1
  fi
}

verifyInstallSummary(){
  text="Do you wish to begin the installation?\n
This may take 30 minutes or more depending on hardware capabilities and network connection speeds.\n\n
System hostname: ${NHOSTNAME}\n
Hard drive: ${DISK}\n
ZFS pool name: ${ZPOOL}\n
SWAP space reserved: ${SWAPSIZE}\n
Create user: ${user} (${usercomment})\n
Package type: ${REPOTYPE}\n
Packages to install: ${PACKAGES_CHROOT}\n
Packages ignored (not available): ${PACKAGES_MISSING}\n
"
  opts=" --yesno \"${text}\" 0 0"
  get_dlg_ans "${opts}"
  exit_err $? "Installation Cancelled"
}

doInstall(){
# Install function. Nothing interactive should ever be in here
# typically piped through tee to log the output

#Print out an install summary really fast (this function is piped into a logfile for later viewing)
echo "Installation Summary
================
System hostname: ${NHOSTNAME}
Hard drive: ${DISK}
ZFS pool name: ${ZPOOL}
SWAP space reserved: ${SWAPSIZE}
Create user: ${user} (${usercomment})
Package type: ${REPOTYPE}
Packages to install: ${PACKAGES_CHROOT}
Packages ignored (not available): ${PACKAGES_MISSING}
================
"
#Create the mountpoint
if [ ! -d "${MNT}" ] ; then
  mkdir -p "${MNT}"
  exit_err $? "Could not create mountpoint directory: ${MNT}"
fi

echo "Starting Installation...

"
echo "-----------------"
echo "Step 1 : Formatting the disk"
echo "-----------------"
echo "Erasing the first 500MB of the disk"
dd if=/dev/zero of=${DISK} bs=100M count=5
echo "Erasing the last 1MB of the disk"
#Note that blockdev returns size in 512 byte blocks
dd if=/dev/zero of=${DISK} bs=512 seek=$(( $(blockdev --getsz ${DISK}) - 2048 )) count=2048

echo "Formatting the disk: ${BOOTMODE} ${DISK}"
sfdisk -w always ${DISK} << EOF
	label: gpt
	,200M,U
	,10M,21686148-6449-6E6F-744E-656564454649,*
	;
EOF
exit_err $? "Could not partition the disk: ${DISK}"
echo "${DISK}" | grep -q "/dev/nvme"
if [ $? -eq 0 ] ; then
  #Need to inject a "p" before the partition number for nvme devices
  DISK="${DISK}p"
fi
EFIDRIVE="${DISK}1"
BOOTDRIVE="${DISK}2"
SYSTEMDRIVE="${DISK}3"


#Formatting the boot partition (FAT32)
mkfs -t msdos ${EFIDRIVE}

# Setup the void tweaks for ZFS 
# Many Steps found at: https://github.com/nightah/void-install
xbps-reconfigure -a
modprobe zfs
exit_err $? "Could not verify ZFS module"

generateHostid
echo "New ISO hostid: $(hostid)"

echo "Creating ZFS Pool: ${ZPOOL}"
zpool create -f -o ashift=12 -d \
		-o feature@async_destroy=enabled \
		-o feature@bookmarks=enabled \
		-o feature@embedded_data=enabled \
		-o feature@empty_bpobj=enabled \
		-o feature@enabled_txg=enabled \
		-o feature@encryption=enabled \
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
		-O encryption=off \
		-O mountpoint=none \
		-O normalization=formD \
		-O relatime=on \
		-O xattr=sa \
		${ZPOOL} ${SYSTEMDRIVE}
exit_err $? "Could not create pool: ${ZPOOL} on ${SYSTEMDRIVE}"
# Try to set the autotrim property (if available on this pool/disk)
#  Do not fail if this errors out
zpool set autotrim=on "${ZPOOL}"

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

datasets="home:usr/home vlog:var/log vtmp:var/tmp docker:var/lib/docker"
for ds in ${datasets}
do
  echo "Creating Dataset: ${ds}"
  zfs create -o compression=lz4 -o mountpoint=/$(echo ${ds} | cut -d : -f 2) ${ZPOOL}/$(echo ${ds} | cut -d : -f 1)
  exit_err $? "Could not create dataset: ${ds}"
done

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
#Copy over the repository keys from the ISO (prevent prompting for acceptance if they match already)
mkdir -p "${MNT}/var/db/xbps/keys"
cp /var/db/xbps/keys/*.plist "${MNT}/var/db/xbps/keys/."
#Copy over any custom repo definitions
mkdir -p "${MNT}/etc/xbps.d"
cp /etc/xbps.d/*.conf "${MNT}/etc/xbps.d/."
#Ensure the trident repo config is installed (if not copied from the ISO itself)
if [ !  -e "${MNT}/etc/xbps.d/trident.conf" ] ; then
  wget "https://project-trident.org/repo/conf/trident.conf" -O "${MNT}/etc/xbps.d/trident.conf"
fi

#NOTE: Do NOT install the ZFS package yet - that needs to run inside chroot for post-install actions.
xbps-install -y -S -r "${MNT}" --repository="${REPO}"
xbps-install -y -r "${MNT}" --repository="${REPO}" base-system zfsbootmenu grub grub-i386-efi grub-x86_64-efi ${PACKAGES}
exit_err $? "Could not install void packages!!"

linuxver=`${CHROOT} xbps-query linux | grep pkgver | cut -d - -f 2 | cut -d . -f 1-2 | cut -d _ -f 1`
echo "Got Linux Version: ${linuxver}"

echo "Symlink /home to /usr/home mountpoint"
rmdir ${MNT}/home
${CHROOT} ln -s /usr/home /home

echo
echo "copying a valid resolv.conf into directory, before chroot to get to the new install"
if [ -e "/etc/resolv.conf" ] ; then
  #Copy the current host resolv.conf (assume it is working)
  cp /etc/resolv.conf ${MNT}/etc/resolv.conf
fi

#Copy over any saved wifi networks from the ISO
cp "/etc/wpa_supplicant/wpa_supplicant.conf" "${MNT}/etc/wpa_supplicant/wpa_supplicant.conf"
#add fstab entry to mount /boot/efi partition
echo "${EFIDRIVE} /boot/efi auto rw,nosuid,noauto,nouser,noexec,sync 0 0" >> ${MNT}/etc/fstab

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
echo 'LANG="en_US.UTF-8"' >> ${MNT}/etc/locale.conf

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
echo "-------------------------------"
echo "Step 3: Installing Packages"
echo "-------------------------------"
echo
mkdir ${MNT}/tmp/pkg-cache
rm ${MNT}/var/cache/xbps/*
# Required packages
if [ "${BOOTMODE}" = "EFI" ] ; then
  #if installing with EFI - need refind
  PACKAGES_CHROOT="refind ${PACKAGES_CHROOT}"
fi

for pkg in zfs cryptsetup pam_zfscrypt ${PACKAGES_CHROOT}
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

# Setup encrypted homedir support via PAM
setupPamCrypt

# Now setup encrypted SWAP on the device
if [ -n "${SWAPSIZE}" ] && [ "0" != "${SWAPSIZE}" ] ; then
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
echo "-------------------------------"
echo "Step 4: Creating user account"
echo "-------------------------------"
echo
createUser

echo
echo "-------------------------------"
echo "Step 5: Enabling Services"
echo "-------------------------------"
echo
for service in ${SERVICES_ENABLED}
do
  if [ ! -e "${MNT}/etc/sv/${service}" ] ; then continue ; fi
  echo " -> ${service}"
  ${CHROOT} ln -s /etc/sv/${service} /var/service/${service}
  exit_err $? "Could not enable service: ${service}"
done

echo
echo "-------------------------------"
echo "Step 6: Setup Bootloader(s)"
echo "-------------------------------"
echo
#Now reinstall grub on the boot device after the reconfiguration
#if [ "zfs" != "$(${CHROOT} grub-probe /)" ] ; then
#  echo "ERROR: Could not verify ZFS nature of /"
#  exit 1
#fi
#Chase down a fix that was also added to the grub auto-scripts
export ZPOOL_VDEV_NAME_PATH=YES
#Get the disk UUID for the boot disk
diskuuid=$(blkid --output export ${SYSTEMDRIVE} | grep -E '^(UUID=)' | cut -d = -f 2  )
echo "Got ZFS pool disk uuid: ${diskuuid}"
#Setup the GRUB configuration
mkdir -p ${MNT}/etc/default
wallpaper=$(ls /root/Trident-wallpaper.*)
wallpaper=$(basename ${wallpaper})
wallfmt=$(echo ${wallpaper} | cut -d . -f 2)
cp "/root/${wallpaper}" "${MNT}/etc/default/grub-splash.${wallfmt}"
# NOTE: zfsbootmenu also reads the grub configuration file for many of it's own settings like timeout
echo "
GRUB_DEFAULT=0
GRUB_TIMEOUT=5
GRUB_DISTRIBUTOR=\"Project-Trident\"
GRUB_CMDLINE_LINUX_DEFAULT=\"quiet loglevel=3 elevator=noop nvidia-drm.modeset=1\"
GRUB_BACKGROUND=/etc/default/grub-splash.${wallfmt}
GRUB_CMDLINE_LINUX=\"root=ZFS=${ZPOOL}/ROOT/${INITBE}\"
GRUB_DISABLE_OS_PROBER=true
GRUB_DISABLE_LINUX_UUID=true
GRUB_DISABLE_LINUX_PARTUUID=true
" > ${MNT}/etc/default/grub
#GRUB_CMDLINE_LINUX=\"root=LABEL=${ZPOOL}\" #Does not know it is ZFS and throws a fit
#GRUB_CMDLINE_LINUX=\"root=UUID=${diskuuid}\"
#GRUB_CMDLINE_LINUX=\"root=ZFS=${ZPOOL}/ROOT/${INITBE}\"


# to see if these help
# grub needs updating after we make changes
#echo "updating grub"
#${CHROOT} update-grub
${CHROOT} zpool set cachefile=/etc/zfs/zpool.cache "${ZPOOL}"
${CHROOT} xbps-reconfigure -f linux${linuxver}
#${CHROOT} lsinitrd -m

echo "Installing GRUB bootloader"
#Stamp GPT loader on disk itself for non-EFI boot-ability
${CHROOT} grub-mkconfig -o /boot/grub/grub.cfg
${CHROOT} grub-install ${BOOTDEVICE}

#Stamp EFI loader on the EFI partition
mkdir -p "${MNT}/boot/efi/EFI/boot/"
if [ "${BOOTMODE}" = "EFI" ] ; then
  installZfsBootMenu
else
  #Create a project-trident directory only
  ${CHROOT} grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=Project-Trident --recheck --no-floppy
  #Copy the EFI registration to the default boot path as well
  cp "${MNT}/boot/efi/EFI/project-trident/grubx64.efi" "${MNT}/boot/efi/EFI/boot/bootx64.efi"
fi
# Take an initial snapshot of the boot environment
${CHROOT} zfs snapshot ${ZPOOL}/ROOT/${INITBE}@cleaninstall
echo
echo "[SUCCESS] Reboot the system and remove the install media to boot into the new system"

} #end of the doInstall function

# ===============
#  LOAD SETTINGS
# ===============
#Check if we are using EFI boot
efibootmgr > /dev/null 2>/dev/null
if [ $? -eq 0 ] ; then
  #Using EFI
  BOOTMODE="EFI"
else
  BOOTMODE="LEGACY"
  get_dlg_ans " --yesno \"WARNING: Please boot with UEFI for the best experience.\n\nThis system is currently booting in legacy mode and features such as boot environments and dataset encryption will not be available.\n\nContinue with install setup anyway?\" 0 0"
  exit_err $? "Installation Cancelled (Legacy boot)"
fi

PAGETOT="8"
while [ -z "${DISK}" ]
do
  PAGENUM="1/${PAGETOT}"
  getDisks
done
if [ -z "${SWAPSIZE}" ] ; then
  PAGENUM="2/${PAGETOT}"
  getSwap
fi
if [ -z "${REPOTYPE}" ] ; then
  PAGENUM="3/${PAGETOT}"
  getRepotype
fi
if [ -z "${ROOTPW}" ] ; then
  PAGENUM="4/${PAGETOT}"
  getPassword "root"
  ROOTPW="${ANS}"
  unset ANS
fi
if [ -z "${NHOSTNAME}" ] ; then
  PAGENUM="5/${PAGETOT}"
  NHOSTNAME="Trident-${RANDOM}"  
  adjustTextValue "Select system hostname" "${NHOSTNAME}"
  NHOSTNAME="${ANS}"
fi
if [ -z "${ZPOOL}" ] ; then
  PAGENUM="6/${PAGETOT}"
  zpool import -aN
  ANS=""
  while [ $? -eq 0 ] || [ -z "${ANS}" ]
  do
    if [ -n "${ANS}" ] ; then
      adjustTextValue "Pool already exists: Select different ZFS pool name" "${ZPOOL}"
    else
      adjustTextValue "Select ZFS pool name" "trident"
    fi
    ZPOOL=$(echo "${ANS}" | tr -d '[:space:]' | tr '[:upper:]' '[:lower:]')
    ANS="${ZPOOL}"
    zpool list "${ZPOOL}"  > /dev/null 2> /dev/null
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
  PAGENUM="7/${PAGETOT}"
  getPackages
fi
if [ -z "${TIMEZONE}" ] ; then
  TIMEZONE="America/New_York"
fi
#Now get user creation info
if [ -n "${PACKAGES_CHROOT}" ] ; then
  PAGENUM="8/${PAGETOT}"
  getUser
fi

# ==============================
#  Generate Internal Variables from settings
# ==============================
BOOTDEVICE="${DISK}"
MNT="/run/ovlwork/mnt"
CHROOT="chroot ${MNT}"
ARCH=$(uname -m)
# Automatically adjust the musl/glibc repo switch as needed
if [ "${REPOTYPE}" = "musl" ] ; then
  export XBPS_ARCH=${ARCH}-musl
  REPO="https://alpha.de.repo.voidlinux.org/current/musl"
else
  export XBPS_ARCH=${ARCH}
  REPO="https://alpha.de.repo.voidlinux.org/current"
fi
checkPackages

#Verify that they want to begin the install now
unset PAGENUM
verifyInstallSummary

# DO NOT Set target arch. This disables a lot of the post-install configuration for packages
#export XBPS_TARGET_ARCH="${XBPS_ARCH}" 
if [ -n "${LOGFILE}" ] ; then
  # Split between log and stdout
  doInstall 2>&1 | tee "${LOGFILE}"
  #Copy the logfile over to the installed system logs
  cp "${LOGFILE}" "${MNT}/var/log/trident-install.log"
else
  # Just use stdout
  doInstall
fi

#Now cleanup before exit
cleanupInstall
