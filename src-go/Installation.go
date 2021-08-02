package main

import (
	"os/exec"
)

func Chroot(mnt string, args []string ) error {
	input := []string{ mnt }
	input = append(input, args...)
	return exec.Command("chroot", args...).Run()
}
