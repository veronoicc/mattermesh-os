// Settings > Backup / restore (59): main view, backup list, confirmation, passphrase editor. Included by renderer.cpp.
#include "backup_layout.h"
void Renderer::backupPage(const State &s){
 const BackupView &v=s.backup;shell(s,v.view==1?"Restore":"Backup / restore");g.fillRect(W-32,StatusH,32,HeaderH-1,BG);
 auto pill=[&](Rect row,const char *label,const char *text,bool on){c.card(row);c.text(row.x+7,row.y+(row.h-15)/2,label,Font::ChatBody,FG,row.w-80);auto p=backupPill(row);if(on)g.fillRoundRect(p.x,p.y,p.w,p.h,11,FG);else g.drawRoundRect(p.x,p.y,p.w,p.h,11,FG);c.centered(p.x,p.y+5,p.w,text,Font::Bold,on?BG:FG);};
 if(v.view==1){
  const int count=int(v.rows.size());
  if(!count){c.centered(Margin,Top+60,W-2*Margin,"No backups on the SD card",Font::ChatBody);c.centered(Margin,Top+82,W-2*Margin,"/mattermesh/backup",Font::ChatMeta);return;}
  for(int slot=0;slot<BackupVisible;slot++){int i=v.scroll+slot;if(i>=count)break;Rect r=backupListRow(slot);bool on=i==v.selected;auto ink=on?BG:FG;c.card(r,on,6);c.text(r.x+8,r.y+7,v.rows[i].when,Font::Bold,ink,r.w-16);c.text(r.x+8,r.y+27,v.rows[i].meta,Font::ChatMeta,ink,r.w-16);}
  for(int i=0;i<2;i++){Rect r=i?BackupNext:BackupPrev;bool can=i?v.scroll<backupMaxScroll(count):v.scroll>0;c.card(r,false,5);int cx=r.x+r.w/2,cy=r.y+r.h/2,d=i?-1:1;if(can)g.fillTriangle(cx-5,cy+3*d,cx+5,cy+3*d,cx,cy-3*d,FG);else g.drawTriangle(cx-5,cy+3*d,cx+5,cy+3*d,cx,cy-3*d,FG);}
  c.text(Margin+2,BackupPrev.y+5,std::to_string(v.scroll+1)+"-"+std::to_string(std::min(count,v.scroll+BackupVisible))+" of "+std::to_string(count),Font::ChatMeta);
  return;}
 if(v.view==2){
  c.card(BackupConfirmCard,false,6);Rect r=BackupConfirmCard;c.text(r.x+8,r.y+8,"Restore this backup?",Font::ChatTitle,FG,r.w-16);c.text(r.x+8,r.y+30,v.title,Font::ChatMeta,FG,r.w-16);
  c.text(r.x+8,r.y+52,"This replaces:",Font::ChatBody,FG,r.w-16);
  for(size_t i=0;i<v.lines.size()&&i<7;i++)c.text(r.x+16,r.y+70+int(i)*14,"- "+v.lines[i],Font::ChatMeta,FG,r.w-24);
  c.text(r.x+8,r.y+r.h-22,"The device restarts when done.",Font::ChatMeta,FG,r.w-16);
  c.button(BackupGo,"RESTORE",v.button==0);c.button(BackupCancel,"CANCEL",v.button==1);return;}
 if(v.view==3){
  c.card(BackupConfirmCard,false,6);c.text(Margin+8,Top+14,v.title,Font::ChatTitle,FG,W-2*Margin-16);c.text(Margin+8,Top+38,v.hint,Font::ChatMeta,FG,W-2*Margin-16);
  c.card(BackupPassCard,true,6);c.text(BackupPassCard.x+8,BackupPassCard.y+(BackupPassCard.h-15)/2,c.inputTail(v.input+"_",BackupPassCard.w-16,Font::Bold),Font::Bold,BG,BackupPassCard.w-16);
  c.text(Margin+8,Top+110,v.status,Font::ChatMeta,FG,W-2*Margin-16);
  c.button(BackupGo,"OK",v.button==0);c.button(BackupCancel,"CANCEL",v.button==1);return;}
 c.toggleRow(BackupCreate,Icon::Files,"Create backup",v.status,v.sd,v.sd?"RUN":"--");
 c.sectionLabel(SettingsSectionY,"OPTIONS");
 pill(BackupPasswords,"Passwords",v.passwords?"ON":"OFF",v.passwords);
 pill(BackupKey,v.keyAvailable?"Private key":"Private key (off)",v.keyAvailable?(v.key?"ON":"OFF"):"N/A",v.keyAvailable&&v.key);
 pill(BackupEncrypt,"Encrypt",v.encrypt?"ON":"OFF",v.encrypt);
 c.sectionLabel(BackupLabelY,"RESTORE");
 c.menuRow(BackupRestore,Icon::Files,"Restore from SD",v.hint);
 for(size_t i=0;i<v.lines.size()&&i<3;i++)c.text(Margin+2,BackupInfoY+int(i)*13,v.lines[i],Font::ChatMeta,FG,W-2*Margin-4);
 {Rect r=backupRowRect(std::max(0,std::min(BackupRows-1,v.focus)));g.fillRect(2,r.y+4,3,r.h-8,FG);}
}
