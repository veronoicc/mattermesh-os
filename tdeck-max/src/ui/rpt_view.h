#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include "rpt_layout.h"
namespace mm {
// One list row: signal and age texts are ready to draw.
struct RptRowView {std::string name,distance,signal,age;bool live=false,contact=true,scanned=false;};
// RPT Scanner state for the renderer, filled by rptCompose (rpt_app.inc). mode: 0 list, 1 repeater page, 2 password, 3 console.
struct RptView {
 int mode=0,sort=0,focus=0,first=0,total=0,defaultSort=0;
 bool radioOn=true,menu=false,picker=false,showRssi=true,scanning=false;
 int menuFocus=0,hideHours=0,listenSec=10,scanLeft=0;
 std::string status;// list state line: scan progress, count or the last problem
 std::vector<RptRowView> rows;// the visible window
 // Repeater page
 std::string name,key,position,advert,signal,path;bool contact=true,loggedIn=false;int actionFocus=0,answerState=0;std::vector<std::string> answer;
 // Password page and console
 std::string input,note;bool masked=true,waiting=false;
 std::vector<std::string> console;int consoleScroll=0;
 // DM PATH picker
 std::vector<std::string> pickRows;int pickFocus=0,pickFirst=0,pickTotal=0;
};
}
