package main

import (
	"os/exec"
	"os"
)

func CreateZfsPool(name string, disks []string) error {
	inputs := []string{
		"create", "-f", 
		"-o", "ashift=12",
		"-O","acltype=posixacl",
		"-O","canmount=off",
		"-O","compression=lz4",
		"-O", "devices=off",
		"-O", "encryption=off",
		"-O", "mountpoint=none",
		"-O", "normalization=formD",
		"-O", "relatime=on",
		"-O", "xattr=sa",
		name,
	}
	inputs = append(inputs, disks...)
	err := exec.Command("zpool", inputs...).Run()
	if err == nil {
		// Ensure to turn on autotrim for SSD devices. 
		// This will fail if no SSD's were used (do not error out on fail)
		_ = exec.Command("zpool", "set", "autotrim=on", name).Run()
	}
	return err
}

func CreateZfsDataset( ds string, opts []string) error {
	args := []string{ "create" }
	for _, opt := range opts {
		args = append(args, "-o")
		args = append(args, opt)
	}
	args = append(args, ds)
	return exec.Command("zpool", args...).Run()
}

func ConfigZfsPoolBE(pool string, initbe string, mountpoint string) error {
	//Setup a pool for Boot Environment (BE) support
	err := CreateZfsDataset(pool+"/ROOT", []string{"canmount=off"} )
	if err != nil { return err }
	BE := pool+"/ROOT/"+initbe
	err = CreateZfsDataset(BE, []string{"mountpoint=/", "canmount=noauto"} )
	if err != nil { return err }
	err = exec.Command("zpool","set","bootfs="+BE, pool).Run()
	if err != nil { return err }
	//Now export/import the pool and mount the BE on the mountpoint instead of the pool itself
	err = exec.Command("zpool","export", pool).Run()
	if err != nil { return err }
	err = exec.Command("zpool","import","-R", mountpoint, pool).Run()
	if err != nil { return err }
	err = exec.Command("zpool","mount", BE).Run()
	if err != nil { return err }
	//Create the initial datasets for a BE pool
	err = CreateZfsDataset( pool+"/home", []string{"compression=lz4", "mountpoint=/usr/home"})
	if err != nil { return err }
	err = CreateZfsDataset( pool+"/vlog", []string{"compression=lz4", "mountpoint=/var/log"})
	if err != nil { return err }
	err = CreateZfsDataset( pool+"/vtmp", []string{"compression=lz4", "mountpoint=/var/tmp"})
	if err != nil { return err }
	err = CreateZfsDataset( pool+"/docker", []string{"compression=lz4", "mountpoint=/var/lib/docker"})
	if err != nil { return err }
	//Create initial BE dirs
	for _, dir := range []string{ "dev", "etc", "proc", "run", "sys" } {
		err = os.MkdirAll(mountpoint+"/"+dir, 0755)
		if err != nil { return err }
	}
	err = MountTmpSysDirs(mountpoint, false) //mount them now
	return err
}

func MountTmpSysDirs(mountpoint string, unmount bool) error {
	//Temporarily mount/unmount the system dirs as "live" in the install environment
	for _, dir := range []string{ "dev", "proc", "run", "sys" } {
		var err error
		if unmount {
			err = exec.Command("umount", "-nfR", mountpoint+"/"+dir).Run()
		}else{
			err = exec.Command("mount", "--rbind", "/"+dir, mountpoint+"/"+dir).Run()
		}
		if err != nil { return err }
	}
	return nil
}

