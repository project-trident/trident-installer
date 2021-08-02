package main

import (
	"os/exec"
)

type InstallSettings struct {
	Mountpoint	string
	PoolName		string
	InitialBE	string
	
	
}


func Chroot(mnt string, args []string ) error {
	input := []string{ mnt }
	input = append(input, args...)
	return exec.Command("chroot", args...).Run()
}
