#pragma once
// A card the firmware cannot mount (emulator --no-sd): EMU_SD_FS=exfat|ntfs gives a present card with that file system,
// otherwise no card answers.
#include "ff.h"
#include <stdlib.h>
#include <string.h>
#define STA_NOINIT 0x01
static inline const char *emuSdFs(){const char *v=getenv("EMU_SD_FS");return v&&*v?v:nullptr;}
static inline DSTATUS disk_initialize(BYTE){return emuSdFs()?0:STA_NOINIT;}
static inline DRESULT disk_read(BYTE,BYTE *b,DWORD sector,UINT){if(!emuSdFs())return RES_NOTRDY;memset(b,0,512);
 if(sector==0){memcpy(b+3,!strcmp(emuSdFs(),"ntfs")?"NTFS    ":"EXFAT   ",8);b[510]=0x55;b[511]=0xAA;}return RES_OK;}
