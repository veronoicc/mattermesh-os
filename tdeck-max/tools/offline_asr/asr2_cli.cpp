// Host transcription with the firmware's MMQIT002 engine (src/offline_it/asr2.cpp), for comparing a quantized model
// with the float reference. Build: g++ -std=c++14 -O2 -Isrc src/offline_it/asr2.cpp tools/offline_asr/asr2_cli.cpp -o asr2_cli
// Usage: asr2_cli model.mmq a.wav [b.wav ...]   (16 kHz mono 16-bit WAV, cut to the device's 6 s)
// Prints "path<TAB>text" per file, then the memory the device needs for this model on stderr.
#include "offline_it/asr2.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
using namespace mm_asr;
struct Image {const uint8_t *data;size_t size;std::vector<uint8_t> staging;};
static bool readImage(void *c,uint32_t offset,void *dst,uint32_t bytes){auto *i=static_cast<Image*>(c);if(size_t(offset)+bytes>i->size)return false;memcpy(dst,i->data+offset,bytes);return true;}
static const uint8_t *blob(void *c,unsigned,const Op &o){
 auto *i=static_cast<Image*>(c);if(size_t(o.blobOffset)+o.storedBytes>i->size)return nullptr;if(!packed(o))return i->data+o.blobOffset;
 if(i->staging.size()<o.blobBytes)i->staging.resize(o.blobBytes);
 memcpy(i->staging.data()+(o.blobBytes-o.storedBytes),i->data+o.blobOffset,o.storedBytes);unpackBlob(o,i->staging.data());return i->staging.data();
}
static bool readWav(const char *path,std::vector<int16_t> &pcm){
 std::ifstream f(path,std::ios::binary);std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)),{});
 if(b.size()<44||memcmp(b.data(),"RIFF",4)||memcmp(b.data()+8,"WAVE",4))return false;
 for(size_t p=12;p+8<=b.size();){uint32_t n;memcpy(&n,b.data()+p+4,4);
  if(!memcmp(b.data()+p,"data",4)){size_t end=std::min<size_t>(b.size(),p+8+n);pcm.resize((end-p-8)/2);memcpy(pcm.data(),b.data()+p+8,pcm.size()*2);return true;}
  p+=8+n+(n&1);}
 return false;
}
int main(int argc,char **argv){
 if(argc<3){fprintf(stderr,"usage: %s model.mmq a.wav...\n",argv[0]);return 2;}
 std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> file((std::istreambuf_iterator<char>(f)),{});
 Image image{file.data(),file.size(),{}};Source source;source.context=&image;source.read=readImage;
 Model model;if(open(model,source)!=Ok||model.bytes!=file.size()){fprintf(stderr,"invalid model %s\n",argv[1]);return 1;}
 alignas(16) static int8_t scratch[ScratchBytes];Hooks hooks;hooks.context=&image;hooks.blob=blob;hooks.quantScratch=scratch;
 const size_t limit=size_t(SampleRate)*6;                    // device policy: DICTATION_MAX_SECONDS
 size_t arena=workspaceBytes(model,limit);std::vector<uint8_t> workspace(arena);
 fprintf(stderr,"%s: file %zu B, %u ops, %u classes, maxBlob %u -> PSRAM per dictation: arena %zu B (6 s) + 2 x staging %u B = %zu B\n",argv[1],file.size(),model.ops,model.classes,model.maxBlob,arena,(model.maxBlob+15)&~15u,arena+2*size_t((model.maxBlob+15)&~15u));
 int status=0;
 for(int a=2;a<argc;a++){
  std::vector<int16_t> pcm;if(!readWav(argv[a],pcm)){fprintf(stderr,"%s: not a PCM WAV\n",argv[a]);status=1;continue;}
  if(pcm.size()>limit)pcm.resize(limit);
  size_t need=workspaceBytes(model,pcm.size());char text[512];Stats stats;
  Error e=transcribe(model,pcm.data(),pcm.size(),workspace.data(),std::min(need,workspace.size()),text,sizeof(text),stats,hooks);
  if(e!=Ok){fprintf(stderr,"%s: error %d\n",argv[a],int(e));status=1;continue;}
  printf("%s\t%s\n",argv[a],text);
 }
 return status;
}
