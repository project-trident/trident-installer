# trident-installer
This provides a Qt5-based graphical front-end to the [pc-sysinstall scriptable installer](https://github.com/trueos/trueos/tree/trueos-master/usr.sbin/pc-sysinstall) used by [TrueOS](https://github.com/trueos/trueos).

This utility is designed to run stand-alone on the installer. It will automatically setup/start Xorg with either the "vesa" or "scfb" driver (based on whether the system is booting in Legacy or UEFI mode), setup and perform the installation through a series of prompts, and then provide the option to shutdown/reboot the system afterwards.

## Bug Reports
Bug reports related to the ISO or the graphical installation procedure should be reported here.
* ***Post-installation*** bug reports should be reported on the [trident-core](https://github.com/project-trident/trident-core) repository.
* ***Package build/release*** bug reports should be reported on the [trident-build](https://github.com/project-trident/trident-build) repository.
* ***Website or documentation*** bug reports should be reported on the [trident-website](https://github.com/project-trident/trident-website) repository.

## Repo Heirarchy:
* **src-qt5**: Source files for the graphical installer itself.
   * *src-qt5/scripts/start-trident-installer* : Initial startup script which runs on the ISO
   * *src-qt5/files/* : Other setup/overlay files which need to be installed in special places on the ISO
   * *src-qt5/packages.json* : JSON file which details all the possible packages which can be selected for installation from the ISO.
* **pkg**: FreeBSD port files
   * *pkg/trident-installer* : Template for the trident/trident-installer FreeBSD port
   * *pkg/mkport.sh* : Script to turn that template into a "real" port pointing at the latest revision of this repository

## How to build the utility
The Trident installer is just a simple Qt5 utility that utilizes the "qmake" build framework.
* **Required Qt5 Modules:** core, gui, widgets, concurrent
* **Required Qt5 Utilities:** qmake, lrelease

Steps to build locally:
* `cd src-qt5`
* `qmake`
* `make`
To install the utility on the local system (hazardous - may overwrite some system files):
* `make install`


#### Testing the utility
Once compiled locally, the `trident-installer` binary can be run on the developer system with user permissions to launch it in "debug mode" (there will be a note at the bottom of the installer which states if it is running in debug mode). This will allow full access to test out the graphical installer (in a window rather than full screen), and will simply skip the "installation" part of the utility and provide a fake 10-second timer in place of the actual installation process (for testing out the while-installing functionality). Similarly, the shutdown/reboot options in the window will just close down the utility rather than initiate the shutdown/reboot processes on the developer system.
