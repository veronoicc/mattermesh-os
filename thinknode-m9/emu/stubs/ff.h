#pragma once
// FatFs types used by main.cpp's SD diagnosis (sdDiagnose).
#include <stdint.h>
typedef uint8_t BYTE;typedef uint32_t DWORD;typedef unsigned UINT;typedef BYTE DSTATUS;
typedef enum{RES_OK=0,RES_ERROR,RES_WRPRT,RES_NOTRDY,RES_PARERR}DRESULT;
