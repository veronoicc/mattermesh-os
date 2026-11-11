#pragma once
#include <Arduino.h>
#include <atomic>
namespace device {
struct OfflineItalianMetrics {bool compiled=false,modelReady=false;uint32_t modelBytes=0,workspaceBytes=0,elapsedMs=0,progress=0,stage=0,op=0,stackMin=0,internalMin=0,psramMin=0,loadMs=0,featuresMs=0,lastOpMs=0,maxOpMs=0,depthwiseMs=0,pointwiseMs=0,cores=0,modelOps=0,residentOps=0,prefixBytes=0,sdBytes=0,sdReadMs=0,sdWaitMs=0,sdReads=0,prefetchHits=0;uint64_t macs=0;int error=0;};
void offlineItalianReset();
// Forget the cached model (after a new file was copied to the SD card). Ignored while transcribing.
bool offlineItalianReload();
// Device diagnostics: cores 0=auto/1/2 for the next inference; trace prints OFFLINE_IT_OP per operator from the worker task.
void offlineItalianDebug(int cores,int trace);
// Prints OFFLINE_IT_BENCH: int8 dot and pointwise block timings with operands in internal RAM vs PSRAM.
OfflineItalianMetrics offlineItalianMetrics();
// language: index into DICTATION_LANGUAGES (services/dictation_language.h); the model is read from its SD path.
int offlineItalianTranscribe(const int16_t *pcm,size_t samples,std::atomic<bool> &cancel,String &text,int language);
String offlineItalianMessage();
// The model file of each language is /mattermesh/asr-<lang>.mmq (dictationModelPath).
}
