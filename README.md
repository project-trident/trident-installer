# trident-installer

## Bug Reports
Bug reports related to the ISO or the graphical installation procedure should be reported here.
* ***Post-installation*** bug reports should be reported on the [trident-core](https://github.com/project-trident/trident-core) repository.
* ***Package build/release*** bug reports should be reported on the [trident-build](https://github.com/project-trident/trident-build) repository.
* ***Website or documentation*** bug reports should be reported on the [trident-website](https://github.com/project-trident/trident-website) repository.

## Repo Heirarchy:
* **src-go**: Source files for the next-generation on-ISO installer (Work-In-Progress).
* **src-sh**: Shell scripts for the installation routines.
   * iso-overlay: File structure to be overlayed within the ISO filesystem during ISO creation.
   * void-build-iso.sh : Script to build an installer ISO
   * void-install-zfs.sh : "Live" script used to perform the actual installation. This is the file automatically fetched/run during the installation routine on the ISO.
