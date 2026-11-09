// Remote CLI of this device (MyMesh::onCLICommandRecv): an opt-in setting plus the contact's remote-CLI flag decide whether
// a command received over the air runs, and only the MeshCore-compatible subset does. Host tested (sim/test_remote_cli.cpp).
#pragma once
#include <cstddef>
#include <cstring>
namespace matter {
constexpr size_t REMOTE_CLI_REPLY=161;// BaseChatMesh builds the reply in 166 bytes after a 5 byte header
using RemoteCliRun=size_t (*)(const char *command,char *reply,size_t cap);// device CLI: reply length, 0 = unknown command
inline bool remoteCliToken(const char *&p,char *out,size_t cap){while(*p==' ')p++;size_t n=0;while(*p&&*p!=' '){if(n+1<cap)out[n]=*p;n++;p++;}if(n>=cap)n=cap-1;out[n]=0;return n>0;}
inline bool remoteCliAllowed(const char *command){
 char verb[16],arg[16];if(!remoteCliToken(command,verb,sizeof(verb)))return false;
 for(const char *v:{"ver","clock","uptime","advert","neighbors","stats-radio","stats-packets","stats-core","reboot"})if(!strcmp(verb,v))return true;
 const bool get=!strcmp(verb,"get");if((!get&&strcmp(verb,"set"))||!remoteCliToken(command,arg,sizeof(arg)))return false;
 for(const char *a:{"name","radio","tx"})if(!strcmp(arg,a))return true;
 if(!get)return false;
 for(const char *a:{"freq","bw","sf","cr","public.key","role","lat","lon"})if(!strcmp(arg,a))return true;
 return false;
}
// The reply for a received command: empty (nothing is sent, nothing runs) unless `enabled` and `flagged`; otherwise the
// device CLI's answer, or "Unknown command". MeshCore's optional "xx|" prefix is reflected in front.
inline size_t remoteCliReply(bool enabled,bool flagged,const char *command,RemoteCliRun run,char *reply){
 reply[0]=0;if(!enabled||!flagged)return 0;
 while(*command==' ')command++;
 size_t pre=0;if(strlen(command)>4&&command[2]=='|'){memcpy(reply,command,3);reply[3]=0;command+=3;pre=3;}
 size_t n=remoteCliAllowed(command)&&run?run(command,reply+pre,REMOTE_CLI_REPLY-pre):0;
 if(!n){strcpy(reply+pre,"Unknown command");n=strlen("Unknown command");}
 return pre+n;
}
}
