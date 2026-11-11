#pragma once
// Dictation language (Settings > Sound / dictation): one MMQIT002 model per language on the SD card.
// Pure logic: no Arduino, no SD access (the caller checks the file and passes presence and size).
#include <stdint.h>
#include <stdio.h>
#include <string>
namespace device {
// Selector order: English, Deutsch, Italiano. Italian is the default: installs that only carry asr-it.mmq keep working.
constexpr int DICTATION_LANGUAGE_COUNT=3,DICTATION_LANGUAGE_DEFAULT=2;
struct DictationLanguageInfo {const char *name,*english,*model,*path;};
constexpr DictationLanguageInfo DICTATION_LANGUAGES[DICTATION_LANGUAGE_COUNT]={
 {"English","English","asr-en.mmq","/mattermesh/asr-en.mmq"},{"Deutsch","German","asr-de.mmq","/mattermesh/asr-de.mmq"},{"Italiano","Italian","asr-it.mmq","/mattermesh/asr-it.mmq"}};
// A stored value outside the list (corrupt NVS, other firmware) means the default, never an out-of-range index.
inline int dictationLanguageValid(int language){return language>=0&&language<DICTATION_LANGUAGE_COUNT?language:DICTATION_LANGUAGE_DEFAULT;}
inline int dictationLanguageStep(int language,int step){int n=DICTATION_LANGUAGE_COUNT;return ((dictationLanguageValid(language)+step)%n+n)%n;}
inline const char *dictationLanguageName(int language){return DICTATION_LANGUAGES[dictationLanguageValid(language)].name;}
inline const char *dictationModelFile(int language){return DICTATION_LANGUAGES[dictationLanguageValid(language)].model;}
inline const char *dictationModelPath(int language){return DICTATION_LANGUAGES[dictationLanguageValid(language)].path;}
// Sound / dictation keys (the keyboard has no trackball): L or D next language, A previous, others 0.
inline int dictationLanguageKey(char c){return c=='l'||c=='d'?1:c=='a'?-1:0;}
inline std::string dictationEngineLabel(int language){return std::string("Offline, ")+DICTATION_LANGUAGES[dictationLanguageValid(language)].english;}
// "Model on SD, 18.0 MB" / "No asr-de.mmq on SD card" (an empty file is no model).
inline std::string dictationModelStatus(int language,bool present,uint64_t bytes){
 if(!present||!bytes)return std::string("No ")+dictationModelFile(language)+" on SD card";
 char text[32];if(bytes>=1048576)snprintf(text,sizeof(text),"Model on SD, %.1f MB",bytes/1048576.0);else snprintf(text,sizeof(text),"Model on SD, %u KB",unsigned((bytes+1023)/1024));
 return text;
}
inline std::string dictationMissingMessage(int language){return std::string(DICTATION_LANGUAGES[dictationLanguageValid(language)].english)+" model not on SD card / draft kept";}
inline std::string dictationInvalidMessage(int language){return std::string(DICTATION_LANGUAGES[dictationLanguageValid(language)].english)+" model file invalid / draft kept";}
}
