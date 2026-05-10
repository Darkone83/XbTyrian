#pragma once
// stdint.h — MSVC 2003 / Xbox RXDK compat stub
// opl.h includes <stdint.h> for uintptr_t, uint32_t, etc.
// MSVC 2003 doesn't ship stdint.h; this stub covers what opl.h needs.
// If the Xbox RXDK already provides stdint.h, this file is simply shadowed
// by the SDK version in the include search order.

typedef unsigned char       uint8_t;
typedef signed   char       int8_t;
typedef unsigned short      uint16_t;
typedef signed   short      int16_t;
typedef unsigned int        uint32_t;
typedef signed   int        int32_t;
typedef unsigned __int64    uint64_t;
typedef signed   __int64    int64_t;

// On Xbox x86 (32-bit), pointer-sized types are 32-bit
typedef unsigned int        uintptr_t;
typedef signed   int        intptr_t;
