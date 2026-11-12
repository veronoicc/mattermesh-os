#include <cassert>
#include <iostream>
#include <vector>
#include "ui/renderer.h"
int main(){
 using namespace mm;
 auto inside=[](Rect r){return r.x>=Margin&&r.x+r.w<=W-Margin&&r.y>=Top&&r.y+r.h<=Bottom;};
 auto apart=[](Rect a,Rect b){return a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y;};
 auto disjoint=[&](const std::vector<Rect> &page){for(size_t i=0;i<page.size();i++){assert(inside(page[i]));for(size_t j=i+1;j<page.size();j++)assert(apart(page[i],page[j]));}};
 // The page: header card, timeline rows and RESEND are inside the content area and apart; texts clear of the targets.
 {std::vector<Rect> page={DeliveryHeader,DeliveryResend};for(int i=0;i<DeliveryRows;i++)page.push_back(deliveryRowRect(i));disjoint(page);
  assert(DeliveryDetailY>=DeliveryHeader.y+DeliveryHeader.h&&DeliveryDetailY+13<=DeliveryListY);
  assert(DeliveryCountY>=deliveryRowRect(DeliveryRows-1).y+deliveryRowRect(DeliveryRows-1).h&&DeliveryCountY+13<=DeliveryResend.y);}
 // The own-message panel: DELIVERY (action 5) shares the RESEND row, all six buttons inside the content area and apart.
 {std::vector<Rect> panel;for(int i=0;i<6;i++)panel.push_back(ownMessageActionRect(i));disjoint(panel);
  assert(ownMessageActionRect(0).y==ownMessageActionRect(5).y&&ownMessageActionRect(0).x<ownMessageActionRect(5).x);
  assert(apart(ownMessageActionRect(5),HeardList)&&apart(ownMessageActionRect(0),HeardList));
  assert(DeliverySection==62);}
 GFXcanvas1 canvas(W,H);Renderer r(canvas);
 auto shot=[&](const State &s){r.draw(s);return std::vector<uint8_t>(canvas.getBuffer(),canvas.getBuffer()+(W+7)/8*H);};
 auto differs=[&](const State &a,const State &b){return shot(a)!=shot(b);};
 State missing;missing.page=Page::Delivery;
 State s=missing;s.delivery.found=true;s.delivery.preview="Hello mesh, anyone there?";s.delivery.summary="Queued";
 s.delivery.rows={{"+0.0 s","QUE","Queued"}};
 assert(differs(missing,s));
 // Summary, detail, rows and the notice each change the page.
 State sum=s;sum.delivery.summary="Delivered via 2 hops";assert(differs(s,sum));
 State det=s;det.delivery.detail="Route: 2 hops  /  3 repeaters";assert(differs(s,det));
 State more=s;more.delivery.rows.push_back({"+1.8 s","TX","Transmitted (2 hops)"});assert(differs(s,more));
 State note=s;note.notice="Resent / queued";assert(differs(s,note));
 State pre=s;pre.delivery.preview="Another message";assert(differs(s,pre));
 // Rows redraw per event kind.
 State a=s,b=s;a.delivery.rows.push_back({"+3 s","RPT","Heard by Hilltop"});b.delivery.rows.push_back({"+3 s","ACK","ACK received via 2 hops"});assert(differs(a,b));
 // A long timeline scrolls: the visible window and the count line follow the offset.
 State full=s;full.delivery.rows.clear();for(int i=0;i<12;i++)full.delivery.rows.push_back({"+"+std::to_string(i)+" s","RPT","Heard by Repeater "+std::to_string(i)});
 State scrolled=full;scrolled.delivery.offset=4;State end=full;end.delivery.offset=8;State beyond=full;beyond.delivery.offset=99;
 assert(differs(full,scrolled)&&differs(scrolled,end)&&!differs(end,beyond));
 // The own-message panel paints DELIVERY: a context message of yours with the panel open.
 State chat;chat.page=Page::Chat;chat.messageActions=true;chat.contextMessage.mine=true;chat.contextMessage.body="Sent text";chat.contextMessage.sender="YOU";
 {r.draw(chat);auto before=shot(chat);bool ink=false;Rect d=ownMessageActionRect(5);for(int y=d.y;y<d.y+d.h&&!ink;y++)for(int x=d.x;x<d.x+d.w;x++)if(canvas.getPixel(x,y)){ink=true;break;}assert(ink);
  State received=chat;received.contextMessage.mine=false;assert(shot(received)!=before);}
 std::cout<<"PASS: delivery UI: touch targets inside the content area and apart (page and own-message panel with DELIVERY), the page redraws with its state and scrolls\n";
}
