#pragma once
#include <algorithm>
#include <cctype>
#include <strings.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
// RPT Scanner: the repeater list. Order, stale filter, labels and the merge of a zero-hop scan into the known repeaters.
// Pure logic (no Arduino), host tested by sim/test_rpt_rank.cpp.
namespace mm {
enum class RptSort : uint8_t {Snr,Heard,Distance,Name};
constexpr int RptSortCount=4;
// What the order needs from one row; ageS ~0u = never heard.
struct RptKey {std::string name;bool snrKnown=false,live=false,distKnown=false;float snr=0;uint32_t ageS=~0u;double meters=0;};
inline bool rptLess(const std::string &a,const std::string &b){size_t n=std::min(a.size(),b.size());for(size_t i=0;i<n;i++){int x=std::tolower((unsigned char)a[i]),y=std::tolower((unsigned char)b[i]);if(x!=y)return x<y;}return a.size()<b.size();}
// Row indices in display order: strongest SNR, most recently heard, nearest or name first; unknown values last, live before stale on a tie, stable.
inline std::vector<int> rptOrder(const std::vector<RptKey> &k,RptSort sort){
 std::vector<int> order(k.size());for(size_t i=0;i<k.size();i++)order[i]=int(i);
 auto before=[&](int a,int b){const RptKey &x=k[a],&y=k[b];
  switch(sort){
  case RptSort::Snr:if(x.snrKnown!=y.snrKnown)return x.snrKnown;if(x.snrKnown&&x.snr!=y.snr)return x.snr>y.snr;break;
  case RptSort::Heard:if(x.ageS!=y.ageS)return x.ageS<y.ageS;break;
  case RptSort::Distance:if(x.distKnown!=y.distKnown)return x.distKnown;if(x.distKnown&&x.meters!=y.meters)return x.meters<y.meters;break;
  case RptSort::Name:if(rptLess(x.name,y.name))return true;if(rptLess(y.name,x.name))return false;break;}
  return x.live&&!y.live;};
 std::stable_sort(order.begin(),order.end(),before);return order;}
// Hide rows not heard for more than hours (0 = show all); a repeater never heard counts as stale.
inline bool rptStale(uint32_t ageS,int hours){return hours>0&&(ageS==~0u||ageS>uint32_t(hours)*3600);}
inline std::string rptAge(uint32_t s){return s==~0u?"--":s<60?std::to_string(s)+"s":s<3600?std::to_string(s/60)+"m":s<48*3600?std::to_string(s/3600)+"h":std::to_string(s/86400)+"d";}
inline std::string rptHops(int hops){return hops<0?"flood":hops==0?"direct":std::to_string(hops)+(hops==1?" hop":" hops");}
// "[ab12cd]": a repeater known only by the start of its key.
inline std::string rptTag(const std::string &hex){std::string out="[";for(size_t i=0;i<hex.size()&&i<6;i++)out+=char(std::tolower((unsigned char)hex[i]));return out+"]";}
// A repeater for the list: key is the full hex key, or the 16 hex prefix of a scan responder not known yet.
struct RptNode {std::string key,name;bool live=false,contact=false,scanned=false,hasPos=false;uint32_t ageS=~0u;float snr=0,rssi=0;int hops=-1;double lat=0,lon=0;};
struct RptHeard {std::string prefix;float snrIn=0,snrOut=0,rssi=0;uint32_t ageS=0;};
inline std::string rptName(const RptNode &n){return n.name.empty()?rptTag(n.key):n.name;}
inline bool rptPrefixOf(const std::string &prefix,const std::string &key){return !prefix.empty()&&key.size()>=prefix.size()&&!strncasecmp(key.c_str(),prefix.c_str(),prefix.size());}
// Scan responders refresh the rows they match (zero hops, this scan's SNR and RSSI, unless the row was heard more recently) and the others are added.
inline std::vector<RptNode> rptMerge(std::vector<RptNode> nodes,const std::vector<RptHeard> &heard){
 for(const auto &h:heard){
  RptNode *row=nullptr;for(auto &n:nodes)if(rptPrefixOf(h.prefix,n.key)||rptPrefixOf(n.key,h.prefix)){row=&n;break;}
  if(!row){RptNode n;n.key=h.prefix;for(char &c:n.key)c=char(std::tolower((unsigned char)c));nodes.push_back(n);row=&nodes.back();row->ageS=~0u;}
  if(h.ageS<=row->ageS){row->live=true;row->scanned=true;row->ageS=h.ageS;row->snr=h.snrIn;row->rssi=h.rssi;row->hops=0;}}
 return nodes;}
}
