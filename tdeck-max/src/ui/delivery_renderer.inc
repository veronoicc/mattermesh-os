// Message Delivery history (section 62): preview and summary card, route line, the timeline, RESEND. Included by renderer.cpp.
void Renderer::deliveryPage(const State &s){
 shell(s,"Delivery",true,Icon::Menu);g.fillRect(ChatMenu.x,ChatMenu.y,ChatMenu.w,ChatMenu.h-1,BG);
 const DeliveryView &v=s.delivery;
 if(!v.found){c.centered(Margin,Top+80,W-2*Margin,"Message not found",Font::ChatTitle);c.centered(Margin,Top+108,W-2*Margin,"It may have been cleared.",Font::ChatMeta);return;}
 {Rect r=DeliveryHeader;c.card(r);c.text(r.x+8,r.y+4,v.preview.empty()?"(message)":v.preview,Font::Bold,FG,r.w-16);c.text(r.x+8,r.y+24,v.summary,Font::ChatMeta,FG,r.w-16);}
 if(!v.detail.empty())c.text(Margin+2,DeliveryDetailY,v.detail,Font::ChatMeta,FG,W-2*Margin-4);
 int n=int(v.rows.size()),first=std::max(0,std::min(v.offset,std::max(0,n-DeliveryRows)));
 for(int i=0;i<DeliveryRows&&first+i<n;i++){const auto &row=v.rows[first+i];Rect r=deliveryRowRect(i);c.card(r);
  c.text(r.x+8,r.y+3,row.when,Font::Bold,FG,60);c.text(r.x+70,r.y+3,row.marker,Font::ChatMeta,FG,r.w-78);c.text(r.x+8,r.y+17,row.text,Font::ChatMeta,FG,r.w-16);}
 if(!s.notice.empty())c.text(Margin+2,DeliveryCountY,s.notice,Font::ChatMeta,FG,W-2*Margin-4);
 else if(n>DeliveryRows)c.text(Margin+2,DeliveryCountY,std::to_string(first+1)+"-"+std::to_string(std::min(first+DeliveryRows,n))+" of "+std::to_string(n)+" / swipe or J K",Font::ChatMeta,FG,W-2*Margin-4);
 c.button(DeliveryResend,"RESEND",true);
}
