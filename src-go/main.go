package main

import (
	"github.com/gdamore/tcell"
	"github.com/rivo/tview"
)
const pageCount = 5
var footer string = "Default Footer"
var header string = "Default Header"
var frame *tview.Frame

func resetFrame(screen tcell.Screen) bool{
  frame.Clear().
	AddText(header,true, tview.AlignCenter, tcell.ColorGreen).
	AddText(footer,false, tview.AlignCenter, tcell.ColorRed)
  return false
}

func main() {
	app := tview.NewApplication()
	list := tview.NewList()
	list.AddItem("Print Footer", "Print to screen", '1', func(){ footer="Footer Printed"; } )
	list.AddItem("Clear Footer", "Draw", '2', func(){ footer=""; } )
	list.AddItem("Print Header", "Print to screen", '3', func(){ header="Header Printed"; } )
	list.AddItem("Clear Header", "Draw", '4', func(){ header=""; } )

	list.AddItem("Quit", "Press to exit", 'q', func(){app.Stop()} )
	frame = tview.NewFrame(list).SetBorders(0,0,1,1,0,0)
        app.SetBeforeDrawFunc( resetFrame )
	if err := app.SetRoot(frame, true).EnableMouse(true).Run(); err != nil {
		panic(err)
	}
}
