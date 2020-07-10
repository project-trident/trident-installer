package main

import (
	"net/http"
	"time"
	"encoding/json"
	"io/ioutil"
	"strings"
	"fmt"
)

type GitHubBranch struct {
	//Ignore everything except the name here
	Name string	`json:"name"`
}

var branches []string

func startNetworkScan(){
  str := "Network: [yellow::bl]SCANNING"
  if str == Frame.status || len(branches) > 0 { return } //already running or finished previously
  Frame.status = str
  go ScanForNetworking()
}

func ScanForNetworking(){
  client := http.Client{ Timeout: time.Second*2}
  request, _ := http.NewRequest(http.MethodGet, "https://api.github.com/repos/project-trident/trident-installer/branches", nil)
  //req.Header.Set("","")
  ok := false
  for i := 0 ; i < 6 ; i++ {
    result, err := client.Do(request)
    if (err != nil){ 
      if(i<9){ time.Sleep(5*time.Second) }
    } else {
      ok = true
      //Add parsing here to read off the available install scripts
      bytes, _ := ioutil.ReadAll(result.Body)
      var reply []GitHubBranch
      err := json.Unmarshal(bytes, &reply)
      if(len(reply) == 0){ fmt.Println(err, string(bytes)) }
      for  _, branch := range(reply) {
        if( branch.Name == "master" || strings.HasPrefix(branch.Name,"release/") ){
          branches = append(branches, branch.Name)
        }
      }
      break; //already got a success - stop here
    }
  }
  //Now update the status in the frame
  if (ok) {
    Frame.status = "Network: [lightgreen::b]OK"
    //Frame.footer = "Branches: " + strings.Join(branches, ", ")
  }else{
    Frame.status = "Network: [red::b]UNAVAILABLE"
  }

  QueueUpdate()
}
