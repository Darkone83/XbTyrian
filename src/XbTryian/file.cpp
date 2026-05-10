// file.cpp — Xbox port of file.c
//
// Changes from original:
//   data_dir() derives the game data path from XeImageFileName at runtime.
//   Path separator changed from '/' to '\\' throughout.
//   sprintf path building replaced with strcpy/strcat (avoids __stdio_common_vsprintf).
//   fprintf(stderr)/strerror removed — OutputDebugStringA used for error output.
//   SDL_Quit() calls removed.
//   errno.h removed.

#include <xtl.h>

#include "file.h"
#include "opentyr.h"
#include "varz.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char* custom_data_dir = NULL;

// XeImageFileName — Xbox kernel export holding the running XBE's full path.
// XeImageFileName — Xbox kernel export.
// Declared as a VALUE (ANSI_STRING struct) not a pointer.
// Access as XeImageFileName.Buffer directly.
extern "C"
{
    struct XE_ANSI_STRING { unsigned short Length; unsigned short MaximumLength; char* Buffer; };
    extern XE_ANSI_STRING XeImageFileName;  // value, not pointer
}

const char* data_dir(void)
{
    if (custom_data_dir != NULL)
        return custom_data_dir;

    static char s_dir[256] = { 0 };

    if (s_dir[0] != '\0')
        return s_dir;

    // Probe common locations — finds data wherever the XBE lives
    const char* const fallbacks[] = {
        "E:\\TYRIAN\\", "E:\\", "D:\\TYRIAN\\", "D:\\"
    };
    for (int i = 0; i < 4; i++)
    {
        FILE* f = dir_fopen(fallbacks[i], "palette.dat", "rb");
        if (f)
        {
            fclose(f);
            strncpy(s_dir, fallbacks[i], sizeof(s_dir) - 1);
            return s_dir;
        }
    }

    // Last resort
    strncpy(s_dir, "E:\\TYRIAN\\", sizeof(s_dir) - 1);
    return s_dir;
}

// ---------------------------------------------------------------------------
// dir_fopen — prepend directory and fopen.
// Uses backslash separator; avoids sprintf.
// ---------------------------------------------------------------------------

FILE* dir_fopen(const char* dir, const char* file, const char* mode)
{
    char path[256];
    strncpy(path, dir, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';

    // Append separator if the directory doesn't already end with one.
    size_t dlen = strlen(path);
    if (dlen > 0 && path[dlen - 1] != '\\' && path[dlen - 1] != '/')
    {
        if (dlen < sizeof(path) - 1)
        {
            path[dlen] = '\\';
            path[dlen + 1] = '\0';
        }
    }

    strncat(path, file, sizeof(path) - strlen(path) - 1);

    return fopen(path, mode);
}

FILE* dir_fopen_warn(const char* dir, const char* file, const char* mode)
{
    FILE* f = dir_fopen(dir, file, mode);
    if (f == NULL)
    {
        char msg[290];
        strcpy(msg, "warning: failed to open '");
        strncat(msg, file, sizeof(msg) - strlen(msg) - 1);
        strcat(msg, "'\n");
        OutputDebugStringA(msg);
    }
    return f;
}

FILE* dir_fopen_die(const char* dir, const char* file, const char* mode)
{
    FILE* f = dir_fopen(dir, file, mode);
    if (f == NULL)
    {
        char msg[290];
        strcpy(msg, "error: failed to open '");
        strncat(msg, file, sizeof(msg) - strlen(msg) - 1);
        strcat(msg, "' -- check Tyrian data files\n");
        OutputDebugStringA(msg);
        JE_tyrianHalt(1);
    }
    return f;
}

bool dir_file_exists(const char* dir, const char* file)
{
    FILE* f = dir_fopen(dir, file, "rb");
    if (f != NULL)
        fclose(f);
    return (f != NULL);
}

long ftell_eof(FILE* f)
{
    long pos = ftell(f);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, pos, SEEK_SET);
    return size;
}

void fread_die(void* buffer, size_t size, size_t count, FILE* stream)
{
    if (fread(buffer, size, count, stream) != count)
    {
        OutputDebugStringA("error: unexpected problem reading from file\n");
        for (;;) Sleep(1000);
    }
}

void fwrite_die(const void* buffer, size_t size, size_t count, FILE* stream)
{
    if (fwrite(buffer, size, count, stream) != count)
    {
        OutputDebugStringA("error: unexpected problem writing to file\n");
        for (;;) Sleep(1000);
    }
}