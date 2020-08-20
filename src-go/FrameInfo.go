package main

import (
	"github.com/gdamore/tcell"
	"github.com/rivo/tview"
	"strings"
	"time"
	"os/exec"
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
  if newpage == "dl_installer" {
    //Download installer version
    FI.page = "Installer Version"
    FI.list.Clear()
    for _, branch := range(branches){
      if(branch == "master"){
        FI.list.AddItem("Master", "[yellow::b]HAZARD:[-] Latest experimental version", rune(0), func(){ FI.ChangePage("") } )
      } else {
        rel := strings.Split(branch,"/")[1]
        FI.list.AddItem(rel, "Release version: "+ rel, rune(0), func(){ FI.ChangePage("") } )
      }
    }
    FI.list.AddItem("Cancel", "Return to the top-level menu", 'X', func(){ FI.ChangePage("") } )    
    FI.pages.SwitchToPage("list")
  } else {
    //Top Menu
    FI.page = "Top Menu"
    FI.list.Clear()
    if(len(branches) > 0){ 
      FI.list.AddItem("Setup Installation", "Proceed through the installation setup wizard.", 'S', func(){ FI.ChangePage("dl_installer") } )
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

}
