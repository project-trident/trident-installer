#!/bin/sh

_isofile="${1}"
_licfile="${2}"
_outdir="${3}"
_version="${4}"

isopath="${_isofile}"

isoname=$(basename "${_isofile}")
isosize=`ls -lh "${isopath}" | cut -d ' ' -f 5`
sigfile="${isoname}.sig.sha512"
md5_file="${isoname}.md5"
sha_file="${isoname}.sha256"


if [ "${_isofile}" != "${_outdir}/${isoname}" ] ; then
  if [ -f "${_outdir}/${isoname}" ] ; then
    rm "${_outdir}/${isoname}"
  fi
  cp "${_isofile}" "${_outdir}/${isoname}"
fi

isopath="${_outdir}/${isoname}"
sign_iso(){
  openssl dgst -sha512 -sign "${_licfile}" -out "${_outdir}/${sigfile}" "${isopath}"
}

generate_checksums(){
  #MD5
  md5sum "${isopath}" > "${_outdir}/${md5_file}"
  md5_sum=`cat "${_outdir}/${md5_file}" | cut -d ' ' -f 1`
  #SHA256
  sha256sum "${isopath}" > "${_outdir}/${sha_file}"
  sha_sum=`cat "${_outdir}/${sha_file}" | cut -d ' ' -f 1`
}

generate_manifest(){
echo "{
\"iso_file\" : \"${isoname}\",
\"iso_size\" : \"${isosize}\",
\"signature_file\" : \"${sigfile}\",
\"md5_file\" : \"${md5_file}\",
\"md5\" : \"${md5_sum}\",
\"sha256_file\" : \"${sha_file}\",
\"sha256\" : \"${sha_sum}\",
\"build_date\" : \"$(date '+%b %d, %Y')\",
\"build_date_time_t\" : \"$(date '+%s')\",
\"version\" : \"${_version}\"
}
" > "${_outdir}/manifest.json"
}

if [ ! -d "${_outdir}" ] ; then
  mkdir -p "${_outdir}"
fi
sign_iso
generate_checksums
generate_manifest
