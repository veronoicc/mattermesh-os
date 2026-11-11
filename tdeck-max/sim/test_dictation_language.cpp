// Dictation language policy: persisted value validation, stepping, model path per language, labels, SD status, and
// UTF-8 safety of German transcripts inserted into the composer.
#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include "services/dictation_language.h"
#include "services/dictation_policy.h"
int main(){using namespace device;
 // Order of the selector: English, Deutsch, Italiano; Italian stays the default of installs that only carry asr-it.mmq.
 assert(DICTATION_LANGUAGE_COUNT==3&&DICTATION_LANGUAGE_DEFAULT==2);
 assert(!strcmp(dictationLanguageName(0),"English")&&!strcmp(dictationLanguageName(1),"Deutsch")&&!strcmp(dictationLanguageName(2),"Italiano"));
 assert(!strcmp(dictationModelPath(0),"/mattermesh/asr-en.mmq")&&!strcmp(dictationModelPath(1),"/mattermesh/asr-de.mmq")&&!strcmp(dictationModelPath(2),"/mattermesh/asr-it.mmq"));
 assert(!strcmp(dictationModelFile(1),"asr-de.mmq"));
 assert(dictationEngineLabel(0)=="Offline, English"&&dictationEngineLabel(1)=="Offline, German"&&dictationEngineLabel(2)=="Offline, Italian");
 // A stored value outside the list (older/newer firmware, corrupt NVS) falls back to the default instead of indexing past the tables.
 for(int bad:{-1,3,99,-2147483647-1,2147483647})assert(dictationLanguageValid(bad)==DICTATION_LANGUAGE_DEFAULT);
 for(int ok=0;ok<DICTATION_LANGUAGE_COUNT;ok++)assert(dictationLanguageValid(ok)==ok);
 // Stepping wraps both ways and accepts any step size.
 assert(dictationLanguageStep(0,1)==1&&dictationLanguageStep(2,1)==0&&dictationLanguageStep(0,-1)==2&&dictationLanguageStep(1,-1)==0&&dictationLanguageStep(1,3)==1&&dictationLanguageStep(0,-4)==2);
 // SD status line: missing file names the file to copy; present shows the size (MB with one decimal, KB below 1 MB).
 assert(dictationModelStatus(1,false,0)=="No asr-de.mmq on SD card");
 assert(dictationModelStatus(0,true,18874368)=="Model on SD, 18.0 MB");
 assert(dictationModelStatus(2,true,14000000)=="Model on SD, 13.4 MB");
 assert(dictationModelStatus(2,true,524288)=="Model on SD, 512 KB");
 assert(dictationModelStatus(2,true,0)=="No asr-it.mmq on SD card");   // an empty file is not a model
 assert(dictationMissingMessage(1)=="German model not on SD card / draft kept"&&dictationInvalidMessage(0)=="English model file invalid / draft kept");
 // German transcripts: umlauts and sharp s are 2-byte UTF-8; the limit never cuts one in half and nothing is rewritten.
 bool trimmed;std::string de="gr\xc3\xb6\xc3\x9f""e f\xc3\xbcr \xc3\xa4pfel";
 assert(appendDictation("",de,96,trimmed)==de&&!trimmed);
 assert(appendDictation("Hallo",de,96,trimmed)=="Hallo "+de&&!trimmed);
 for(size_t max=0;max<de.size()+8;max++){auto out=appendDictation("Hi",de,max,trimmed);
  assert(out.compare(0,2,"Hi")==0&&out.size()<=std::max<size_t>(max,2));
  std::string tail=out.size()>3?out.substr(3):"";assert(de.compare(0,tail.size(),tail)==0);         // only a prefix of the transcript
  for(size_t i=0;i<tail.size();i++){unsigned char ch=tail[i];if(ch>=0xc0){size_t n=ch>=0xf0?4:ch>=0xe0?3:2;assert(i+n<=tail.size());}}}  // no lead byte cut off
 std::cout<<"PASS: dictation language ids, paths, labels, SD status, wrapping, German UTF-8 insertion\n";
}
