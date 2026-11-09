#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include "mesh/remote_cli.h"
// The device CLI stand-in: records what ran and answers "OUT:<command>" cut to the room it is given.
static std::string last;static int runs=0;static size_t answer=1;
static size_t device(const char *command,char *reply,size_t cap){runs++;last=command;if(!answer)return 0;std::string out="OUT:"+last;size_t n=std::min(out.size(),cap-1);memcpy(reply,out.data(),n);reply[n]=0;return n;}
static std::string ask(bool enabled,bool flagged,const char *command,int *ran=nullptr){char reply[matter::REMOTE_CLI_REPLY];memset(reply,'x',sizeof(reply));int before=runs;size_t n=matter::remoteCliReply(enabled,flagged,command,device,reply);if(ran)*ran=runs-before;assert(strlen(reply)==n&&n<sizeof(reply));return reply;}
int main(){
 int ran;
 // Both the setting and the contact's flag are needed; otherwise nothing runs and nothing is answered.
 assert(ask(false,true,"ver",&ran).empty()&&!ran);
 assert(ask(true,false,"ver",&ran).empty()&&!ran);
 assert(ask(false,false,"ver",&ran).empty()&&!ran);
 assert(ask(true,true,"ver",&ran)=="OUT:ver"&&ran==1);
 // The MeshCore-compatible subset runs; arguments are passed on.
 for(const char *c:{"ver","clock","uptime","get name","get radio","get freq","get bw","get sf","get cr","get tx","get public.key","get role","get lat","get lon","set name Foo Bar","set radio 869.618,62.5,8,8","set tx 14","advert","neighbors","stats-radio","stats-packets","stats-core","reboot","get   name"}){assert(ask(true,true,c,&ran)==std::string("OUT:")+c&&ran==1);}
 assert(ask(true,true,"  get name",&ran)=="OUT:get name"&&ran==1);// leading spaces are skipped, as MyMesh does
 // The device-local commands and anything else are unknown and never reach the device CLI.
 for(const char *c:{"clear","help","log start","log stop","gps","gps on","board","get","get foo","set","set gps on","versions","getname","xyzzy","get/name",""}){assert(ask(true,true,c,&ran)=="Unknown command"&&!ran);}
 // A command the device does not know is unknown as well.
 answer=0;assert(ask(true,true,"ver",&ran)=="Unknown command"&&ran==1);answer=1;
 // MeshCore's optional "xx|" prefix is reflected in front of the answer.
 assert(ask(true,true,"ab|ver",&ran)=="ab|OUT:ver"&&ran==1&&last=="ver");
 assert(ask(true,true,"ab|clear",&ran)=="ab|Unknown command"&&!ran);
 assert(ask(true,false,"ab|ver",&ran).empty()&&!ran);
 // The answer never outgrows the reply buffer of BaseChatMesh (166 bytes minus the 5 byte header).
 char big[200];memset(big,'a',sizeof(big));memcpy(big,"set name ",9);big[199]=0;assert(ask(true,true,big).size()<=matter::REMOTE_CLI_REPLY-1);
 big[0]='z';big[1]='z';big[2]='|';memcpy(big+3,"set name ",9);assert(ask(true,true,big).size()<=matter::REMOTE_CLI_REPLY-1);
 puts("PASS: remote CLI needs the setting and the contact flag; the MeshCore subset runs, the rest is unknown; prefix reflected; reply fits");
}
