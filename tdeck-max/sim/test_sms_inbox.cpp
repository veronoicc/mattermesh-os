#include <cassert>
#include <iostream>
#include "services/sms_inbox.h"
using namespace device;using modem_at::ParsedSms;
static ParsedSms sms(int i,bool unread,const char *num,const char *scts,const char *text){ParsedSms p;p.index=i;p.unread=unread;p.number=num;p.scts=scts;p.text=text;return p;}
int main(){
 SmsInbox box;
 // First listing after boot: nothing is announced, unread ones stay unread, newest first by time not by slot.
 auto fresh=box.merge({sms(1,false,"+39111","26/11/10,09:00:00+04","old"),sms(2,true,"+39222","26/11/11,10:20:30+04","new"),sms(3,false,"+39333","26/11/11,08:00:00+04","mid")});
 assert(fresh.empty()&&box.list.size()==3&&box.list[0].index==2&&box.list[1].index==3&&box.list[2].index==1);
 assert(box.list[0].unread&&!box.list[1].unread&&box.unreadCount()==1&&box.list[0].time=="11/11 10:20"&&box.unreadHashes.size()==1);
 // The module lists a message unread only once: the next listing says read, the cache still says unread.
 box.merge({sms(1,false,"+39111","26/11/10,09:00:00+04","old"),sms(2,false,"+39222","26/11/11,10:20:30+04","new"),sms(3,false,"+39333","26/11/11,08:00:00+04","mid")});
 assert(box.list[0].unread&&box.unreadCount()==1);
 // A new message announces itself exactly once, even when listed again.
 auto msgs=std::vector<ParsedSms>{sms(1,false,"+39111","26/11/10,09:00:00+04","old"),sms(2,false,"+39222","26/11/11,10:20:30+04","new"),sms(3,false,"+39333","26/11/11,08:00:00+04","mid"),sms(4,true,"+39444","26/11/11,11:00:00+04","fresh")};
 fresh=box.merge(msgs);assert(fresh.size()==1&&fresh[0].number=="+39444"&&box.unreadCount()==2&&box.list[0].index==4);
 msgs[3].unread=false;fresh=box.merge(msgs);assert(fresh.empty()&&box.unreadCount()==2);
 // Opening clears one; deleting forgets the other; a restart with saved hashes keeps what was unread.
 assert(box.markRead(4)&&!box.markRead(4)&&box.unreadCount()==1&&box.unreadHashes.size()==1);
 SmsInbox again;again.unreadHashes=box.unreadHashes;again.merge(msgs);assert(again.unreadCount()==1&&again.list[1].unread&&again.list[1].index==2);
 box.remove(2);assert(box.list.size()==3&&box.unreadCount()==0&&box.unreadHashes.empty());
 // Hashes of deleted messages are forgotten; the same text from another number is another message; the set is bounded.
 SmsInbox b;b.unreadHashes={smsHash("x","y","z")};b.merge({sms(1,false,"+1","26/11/11,10:00:00+04","t")});assert(b.unreadHashes.empty());
 assert(smsHash("+1","a","b")!=smsHash("+2","a","b")&&smsHash("ab","","")!=smsHash("a","b",""));
 SmsInbox c;std::vector<ParsedSms> many;for(int i=1;i<=80;i++)many.push_back(sms(i,true,"+39","26/11/11,10:00:00+04",std::to_string(i).c_str()));c.merge(many);assert(c.unreadHashes.size()==SmsUnreadMax);
 // Malformed time stamps sort last and show no time; equal times sort by slot, newest slot first.
 SmsInbox d;d.merge({sms(1,false,"+1","garbage","a"),sms(2,false,"+1","26/11/11,10:00:00+04","b"),sms(3,false,"+1","26/11/11,10:00:00+04","c")});assert(d.list[0].index==3&&d.list[1].index==2&&d.list[2].index==1&&d.list[2].time.empty());
 std::cout<<"PASS: sms inbox merge, unread persistence, announce-once, ordering and bounds\n";
}
