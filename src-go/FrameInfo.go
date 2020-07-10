package main

import (
	"github.com/gdamore/tcell"
	"github.com/rivo/tview"
	"strings"
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
	list *tview.List
}

func (FI *FrameInfo)Init(){
  FI.list = tview.NewList()
  FI.frame = tview.NewFrame(FI.list).SetBorders(0,0,0,0,0,0)
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
  AddText("  "+FI.page,true, tview.AlignLeft, tcell.ColorWhite).
  AddText(FI.pageinfo,true, tview.AlignCenter, tcell.ColorWhite).

  AddText(FI.footer,false, tview.AlignCenter, tcell.ColorWhite)
}

func (FI *FrameInfo) ChangePage( newpage string ){
  if newpage == "dl_installer" {
    //Download installer version
    FI.page = "Installer Version"
    Frame.list.Clear()
    for _, branch := range(branches){
      if(branch == "master"){
        Frame.list.AddItem("Master", "[yellow::b]HAZARD:[-] Latest experimental version", rune(0), func(){ Frame.ChangePage("") } )
      } else {
        rel := strings.Split(branch,"/")[1]
        Frame.list.AddItem(rel, "Release version: "+ rel, rune(0), func(){ Frame.ChangePage("") } )
      }
    }
    Frame.list.AddItem("Cancel", "Return to the top-level menu", 'X', func(){ Frame.ChangePage("") } )    
  } else {
    //Top Menu
    FI.page = "Top Menu"
    Frame.list.Clear()
    if(len(branches) > 0){ 
      Frame.list.AddItem("Setup Installation", "Proceed through the installation setup wizard.", 'S', func(){ Frame.ChangePage("dl_installer") } )
    }else{
      Frame.list.AddItem("Rescan Network", "Check to see if the network setting are working", 's', func(){ startNetworkScan() } )
    }
    Frame.list.AddItem("Reboot", "Reboot the system", 'R', func(){ } )
    Frame.list.AddItem("Poweroff", "Poweroff the system", 'P', func(){ } )
    Frame.list.AddItem("Exit to Terminal", "Drop to the system terminal for manual configuration", 't', func(){app.Stop()} )
  }

}
