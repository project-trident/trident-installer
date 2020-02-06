#!/bin/bash

TARGET_ARCH="$1"
if [ -z "${ARCH}" ] ; then
  TARGET_ARCH="x86_64"
fi
#Verify that needed packages are installed
packages="git qemu-user-static liblz4 make"
for pkg in ${packages}
do
  xbps-query -l | grep -q "ii ${pkg}-"
  if [ $? != 0 ] ; then
    echo "Installing Package: ${pkg}"
    xbps-install -y ${pkg}
  fi
done

CURDIR=`cd $(dirname ${0}) && pwd`

#Fetch the void linux ISO build repository
repodir="${HOME}/.voidbuild"
if [ ! -d "${repodir}" ] ; then
  git clone https://github.com/void-linux/void-mklive ${repodir}
else
  cd ${repodir} && git pull
fi
cd ${repodir} && make
#Fix the initial size estimate 
sed -i 's|+ROOTFS_SIZE/6|+ROOTFS_SIZE|g' ${repodir}/mklive.sh

outdir="${HOME}/void-build-iso"
if [ ! -d "${outdir}" ] ; then
  mkdir -p ${outdir}
fi

readonly ARCH="${TARGET_ARCH}-musl"
readonly DATE=$(date +%Y%m%d)
readonly IMGNAME="Trident-netinstall-${TARGET_ARCH}.iso"

readonly GRUB_PKGS="grub-i386-efi grub-x86_64-efi"
readonly BASE_PKGS="dialog mdadm ${GRUB_PKGS}"
readonly TRIDENT_PKGS="${BASE_PKGS} zfs wget curl mtools gptfdisk"
readonly BLTITLE="Project-Trident-Installer"
# Make sure the output ISO file does not already exist (repeated builds)
if [ -e "${outdir}/${IMGNAME}" ] ; then
  echo "Removing stale image: ${outdir}/${IMGNAME}"
  rm "${outdir}/${IMGNAME}"
fi
#Undocumented - set the environment variable to change the grub wallpaper
export SPLASH_IMAGE="${CURDIR}/iso-overlay/root/Trident-wallpaper.png"
cd ${repodir} && ./mklive.sh -a ${ARCH} -T "${BLTITLE}" -I "${CURDIR}/iso-overlay" -o "${outdir}/${IMGNAME}" -p "${TRIDENT_PKGS}" $@ > >(tee "${outdir}/log.txt") 2> >(tee "${outdir}/err.txt")
if [ $? -eq 0 ] ; then
  echo "ISO Generated: ${outdir}/${IMGNAME}"
else
  echo "Unable to create ISO"
fi
