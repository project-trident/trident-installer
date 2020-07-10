package main

import (
	"github.com/gdamore/tcell"
	"github.com/rivo/tview"
)

func scanForNetwork(){
	
}

var app *tview.Application
var Frame FrameInfo

func BeforeDraw(screen tcell.Screen) bool{
  Frame.Update()
  return false //always return false to allow it to render
}

//Shortcut for an asynchronous update of the window
func QueueUpdate() {
  app.QueueUpdateDraw( Frame.Update )
}

func main() {
	app = tview.NewApplication()
	Frame.Init()
	Frame.ChangePage("") //top menu
	startNetworkScan()
        app.SetBeforeDrawFunc( BeforeDraw )
	if err := app.SetRoot(Frame.frame, true).EnableMouse(true).Run(); err != nil {
		panic(err)
	}
}
