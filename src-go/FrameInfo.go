package main

import (
	tcell "github.com/gdamore/tcell/v2" 
	"github.com/rivo/tview"
	"strings"
	"time"
	"os/exec"
	"strconv"
)

/*NOTE=============
Color codes are [fontcolor:backgroundcolor:flag]
"-" resets back to default
Flags:
  "b" Bold text
  "l"  Blinking text
  "d" Dim text
  "u"  Underline text
==================*/

var ISet InstallSettings
var DiskList map[string]*sfDisk

type FrameInfo struct {
	//Variables which are used to display to the user
	footer string
	page string
	status string
	pageinfo string
	//Access to the display widgets themselves
	frame *tview.Frame
	pages *tview.Pages
	list *tview.List
}

func (FI *FrameInfo)Init(){
  FI.list = tview.NewList()
  FI.pages = tview.NewPages()
  FI.pages.AddPage("list", FI.list, true, true)
  FI.frame = tview.NewFrame(FI.pages).SetBorders(0,0,0,0,0,0)
  //Setup all the colors
  FI.frame.SetBackgroundColor(tcell.ColorBlue)
  FI.list.SetBackgroundColor(tcell.ColorSilver)
  FI.list.SetMainTextColor(tcell.ColorBlack)
  FI.list.SetSecondaryTextColor(tcell.ColorGrey)
  FI.list.SetShortcutColor(tcell.ColorBlack)
  FI.list.SetSelectedBackgroundColor(tcell.ColorGreen)
  FI.list.SetSelectedTextColor(tcell.ColorBlack)
  FI.list.SetHighlightFullLine(false)
  //Setup the default menu info
  FI.status = "Network [red::l]UNAVAILABLE [lightgreen::-]OK"
  FI.page = "Top Menu"
  FI.pageinfo = "Please select an option to get started"
  FI.footer = "www.project-trident.org"
}

func (FI FrameInfo)Update(){
  FI.frame.Clear().
  AddText("",true, tview.AlignCenter, tcell.ColorWhite).
  AddText("[::bu]Project Trident Installer", true, tview.AlignCenter, tcell.ColorLightGreen).
  AddText(FI.status+"  ",true, tview.AlignRight, tcell.ColorWhite).
  AddText(time.Now().Format("Mon, Jan 02, 2006")+"  ",true, tview.AlignRight, tcell.ColorWhite).
  AddText("  "+FI.page,true, tview.AlignLeft, tcell.ColorWhite).
  AddText(FI.pageinfo,true, tview.AlignCenter, tcell.ColorWhite).

  AddText(FI.footer,false, tview.AlignCenter, tcell.ColorWhite)
}

func (FI *FrameInfo) ChangePage( newpage string ){
  //This is where we match page Index to the associated function to populate the page
  switch newpage {
	case "dl_installer": FI.ShowSelectInstallerList()
	case "disk_select": FI.SelectInstallDisk()
	default: FI.ShowTopMenu()
  }

}

func (FI *FrameInfo) ShowTopMenu(){
    FI.page = "Top Menu"
    FI.pageinfo = ""
    FI.list.Clear()
    if(len(branches) > 0){ 
      FI.list.AddItem("Setup Installation", "Proceed through the installation setup wizard.", 'S', func(){ FI.ChangePage("disk_select") } )
    }else{
      FI.list.AddItem("Rescan Network", "Check to see if the network setting are working", 's', func(){ startNetworkScan() } )
    }
    FI.list.AddItem("Reboot", "Reboot the system", 'R', func(){ 
      exec.Command("reboot").Run()
      app.Stop()
    } )
    FI.list.AddItem("Poweroff", "Poweroff the system", 'P', func(){
      exec.Command("poweroff").Run()
      app.Stop()
    } )
    FI.list.AddItem("Exit to Terminal", "Drop to the system terminal for manual configuration", 't', func(){app.Stop()} )
    FI.pages.SwitchToPage("list")
}

func (FI *FrameInfo) SelectInstallDisk(){
	FI.page = "Disk Selection"
	FI.pageinfo = "Select a hard disk for installation."
	FI.list.Clear()
	if DiskList == nil {
		DiskList = ListDisks()
	}
	for disknode, disk := range DiskList {
		//Add the disk itself here
		FI.list.AddItem( disknode +" (Whole Disk)", disk.Model+" ("+disk.Size+")", rune(0), func(){ FI.SelectWholeDisk(disk.Node) } )
		//Now add any partitions here
		for partnode, part := range disk.Partitions {
			FI.list.AddItem( "   "+partnode +" (Partition Only)", "   "+part.Type+" ("+part.Size+")", rune(0), func(){ FI.SelectPartition(part.Node) } )
		}

	}
	FI.list.AddItem("Refresh List", 	"Re-scan the system disks for options", 'r', FI.RefreshDiskList )
	FI.list.AddItem("Exit", "Go back to the top menu", 'q', func(){ FI.ChangePage("")} )
}

func (FI *FrameInfo) RefreshDiskList() {
	DiskList = ListDisks()
}

func (FI *FrameInfo) SelectWholeDisk(disknode string) {
	//TODO
}

func (FI *FrameInfo) SelectPartition(partnode string) {
	//TODO
}

func (FI *FrameInfo) ShowSelectInstallerList(){
    FI.page = "Installer Selection"
    FI.pageinfo = "Select a version of the install routine to use."
    FI.list.Clear()
    gotrelease := false
    for i := len(branches)-1; i>=0; i-- { //
      branch := branches[i]
      if(branch == "master"){
        //always show the "master" version
        FI.list.AddItem("Master", "[yellow::b]HAZARD:[-] Experimental version", rune(0), func(){ FI.ChangePage("") } )
      } else if !gotrelease {
        //only show the latest release version - could have lots of different branches of this which get obsolete
        gotrelease = true
        rel := strings.Split( strings.Split(branch,"/")[1], ".") //Always [YY, MM]
	year := "20"+rel[0]
        monthnum, _ := strconv.Atoi(rel[1])
	month := time.Month( monthnum ).String()
        FI.list.AddItem(rel[0]+"."+rel[1], "Release version: "+ month +", "+year, rune(0), func(){ FI.ChangePage("") } )
      }
    }
    FI.list.AddItem("Cancel", "Return to the top-level menu", 'X', func(){ FI.ChangePage("") } )    
    FI.pages.SwitchToPage("list")
}
