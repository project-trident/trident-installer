package main

import (
	"os/exec"
	"fmt"
	"strings"
	"strconv"
	"regexp"
	"path/filepath"
)

type sfDisk struct {
	Node string	// "/dev/sda"
	Model string
	Size string
	bytes int
	sectors int
	bytesPerSector int
	Type string	//should be "gpt" for gpt partition tables
	UUID string
	Partitions map[string]sfPart

}

type sfPart struct {
	Node string // "/dev/sda2"
	Start int	//in bytes
	End int	//in bytes
	Sectors int
	Size string	//human readable
	Type string	//human readable
}

func ListDisks() map[string]*sfDisk {
	out := make(map[string]*sfDisk)
	data, err := exec.Command("sfdisk", "-l").Output()
	if err != nil { fmt.Println("Got sfdisk error:", err); return out }
	var CD *sfDisk //current disk
	inparts := false
	for _, line := range strings.Split( string(data), "\n") {
		if inparts && !strings.HasPrefix(line, "/") {
			inparts = false
		}
		//fmt.Println(": "+line)
		if strings.HasPrefix(line, "Disk /") {
			//Start of a new disk section
			CD = new(sfDisk)
			//Grab the node path and size info from this line
			CD.Node = strings.Split(strings.Split(line,": ")[0] , " ")[1]
			list := strings.Split( strings.Split(line, ": ")[1], ", ")
			CD.Size = list[0]
			CD.bytes, _ = strconv.Atoi( strings.Split(list[1]," ")[0] )
			CD.sectors, _ = strconv.Atoi( strings.Split(list[2]," ")[0] )
			//Finally ensure this disk is in the output map
			out[ filepath.Base(CD.Node) ] = CD

		}else if strings.HasPrefix(line, "Disk model:"){
			CD.Model = strings.Split( line, "model: ")[1]

		}else if strings.HasPrefix(line, "Disk identifier:"){
			CD.UUID = strings.Split( line, "identifier: ")[1]

		}else if strings.HasPrefix(line, "Disklabel type:"){
			CD.Type = strings.Split( line, "type: ")[1]

		}else if strings.HasPrefix(line, "Sector size"){
			//Note: This is just the physical sector size, not logical sector size
			CD.bytesPerSector, _ = strconv.Atoi( strings.Split( strings.Split( line, "bytes / ")[1], " ")[0] )

		}else if strings.HasPrefix(line, "Device"){
			inparts = true

		}else if inparts {
			space := regexp.MustCompile(`\s+`)
			line = space.ReplaceAllString(line, " ")
			pieces := strings.SplitN(line, " ", 6)
			//fmt.Println("Got partition:", line, pieces, len(pieces) )
			var P sfPart
			P.Node = pieces[0]
			P.Start, _ = strconv.Atoi(pieces[1])
			P.End, _ = strconv.Atoi(pieces[2])
			P.Sectors, _ = strconv.Atoi(pieces[3])
			P.Size = pieces[4]
			P.Type = pieces[5]
			if CD.Partitions == nil { CD.Partitions = make(map[string]sfPart) }
			CD.Partitions[ filepath.Base(P.Node) ] = P
		}
	}
	for key, val := range out {
		if val.UUID == "" {
			//Not a "physical" disk, but just some logical thing at runtime of the install environment
			delete(out, key)
		}
	}
	return out
}

func PartitionDisk(disknode string, parts []string) error {
	// Note: "parts" list entries needs to be in sfdisk format:
	// Example: "500M,U" for a 500 MB EFI partition
	// Example: "*,L" for a Linux data partition on rest of disk

	cmd := exec.Command("sfdisk","--force", "-w", "always", disknode)
	//Assemble the partition list for input
	var input []string
	input = append(input, "label: gpt")
	for _, L := range parts {
		if !strings.HasPrefix(L,",") { L = ","+L }
		input = append(input, L)
	}
	// Attach it as the stdin for the process
	cmd.Stdin = strings.NewReader( strings.Join(input,"\n") )
	//Now run the command
	return cmd.Run()
}

func EraseDisk(disknode string) error {
	dat, err := exec.Command("blockdev", "--getsz", disknode).Output() //this is in 512 byte blocks
	if err != nil { return err }
	blocks, err := strconv.Atoi( string(dat) )
	if err != nil { return err }
	//Erase first bit of the disk (500MiB)
	err = exec.Command("dd", "if=/dev/zero","of="+disknode, "bs=100M", "count=5").Run()
	if err != nil { return err }
	//Erase last bit of the disk (1 MiB)
	err = exec.Command("dd", "if=/dev/zero","of="+disknode, "bs=512", "seek="+strconv.Itoa(blocks-2048), "count=2048" ).Run()
	if err != nil { return err }
	return nil
}

func FormatAsFAT32(disknode string) error {
	//This needs to be run on any new EFI partition before it can be used
	return exec.Command("mkfs","-t","msdos", disknode).Run()
}
