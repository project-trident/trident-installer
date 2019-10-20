#!/bin/bash

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

CURDIR=`dirname ${0}`

#Fetch the void linux ISO build repository
repodir="${HOME}/.voidbuild"
if [ ! -d "${repodir}" ] ; then
  git clone https://github.com/void-linux/void-mklive ${repodir}
else
  cd ${repodir} && git pull
fi
cd ${repodir} && make

outdir="${HOME}/void-build-iso"
if [ ! -d "${outdir}" ] ; then
  mkdir -p ${outdir}
fi

readonly ARCH="x86_64-musl"
readonly DATE=$(date +%Y%m%d)
readonly IMGNAME="trident-netinstall-${DATE}.iso"

readonly GRUB_PKGS="grub-i386-efi grub-x86_64-efi"
readonly BASE_PKGS="dialog dialogbox mdadm ${GRUB_PKGS}"
readonly TRIDENT_PKGS="${BASE_PKGS} zfs wget mtools gptfdisk"

# Make sure the output ISO file does not already exist (repeated builds)
if [ -e "${outdir}/${IMGNAME}" ] ; then
  echo "Removing stale image: ${outdir}/${IMGNAME}"
  rm "${outdir}/${IMGNAME}"
fi

cd ${repodir} && ./mklive.sh -a ${ARCH} -I "${CURDIR}/iso-overlay" -o "${outdir}/${IMGNAME}" -p "${TRIDENT_PKGS}" $@ | tee "${OUTDIR}/log.txt"
if [ $? -eq 0 ] ; then
  echo "ISO Generated: ${outdir}/${IMGNAME}"
else
  echo "Unable to create ISO"
fi
