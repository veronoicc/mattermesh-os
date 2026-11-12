#pragma once
#include <algorithm>
namespace mm {
// Move within the actual grid, without wrapping across row edges.
inline int moveAppFocus(int focus,char key,int count,int columns){
 if(count<=0||columns<=0)return -1;
 if(focus<0||focus>=count)return 0;
 int next=focus;
 if(key=='w')next=focus-columns;
 else if(key=='s')next=focus+columns;
 else if(key=='a'&&focus%columns)next=focus-1;
 else if(key=='d'&&focus%columns<columns-1)next=focus+1;
 return next>=0&&next<count?next:focus;
}
inline bool directionKey(char key){return key=='w'||key=='a'||key=='s'||key=='d';}
// Launcher pages of perPage tiles (3x3): the page follows the focused tile, W/S step a whole row across pages, A/D stay in the row.
inline int appPageCount(int count,int perPage){return count>0?(count+perPage-1)/perPage:1;}
inline int appPage(int focus,int perPage){return focus<0?0:focus/perPage;}
inline int moveLauncherFocus(int focus,char key,int count,int columns,int perPage){
 if(count<=0)return -1;
 if(focus<0||focus>=count)return 0;
 if(key=='w')return focus>=columns?focus-columns:focus;
 if(key=='s')return (focus/columns+1)*columns<count?std::min(focus+columns,count-1):focus;
 if(key=='a')return focus%columns?focus-1:focus;
 if(key=='d')return focus%columns<columns-1&&focus+1<count?focus+1:focus;
 return focus;
}
// Swipe up (forward) shows the next page, down the previous one, keeping the tile position (or the last tile); the same focus when there is no such page.
inline int launcherSwipe(int focus,int count,int perPage,bool forward){
 int page=appPage(focus,perPage),next=page+(forward?1:-1);if(next<0||next>=appPageCount(count,perPage))return focus<0?0:focus;
 return std::min(count-1,next*perPage+(focus<0?0:focus%perPage));
}
}
