package main

import (
	"github.com/gdamore/tcell"
	"github.com/rivo/tview"
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
	//Internal variables for triggering changes
	gotoPage string
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
  FI.pageinfo = "Please Select an option to get started"
  FI.footer = ""
}
func (FI FrameInfo)Update(){
  FI.frame.Clear().
  AddText("",true, tview.AlignCenter, tcell.ColorWhite).
  AddText("[::bu]Project Trident Installer", true, tview.AlignCenter, tcell.ColorLightGreen).
  //AddText("Network [red]UNAVAILABLE [lightgreen]OK ",true, tview.AlignRight, tcell.ColorWhite).
  AddText(FI.status+"  ",true, tview.AlignRight, tcell.ColorWhite).
  AddText("  "+FI.page,true, tview.AlignLeft, tcell.ColorWhite).
  AddText(FI.pageinfo,true, tview.AlignCenter, tcell.ColorWhite).

  AddText(FI.footer,false, tview.AlignCenter, tcell.ColorWhite)
}
