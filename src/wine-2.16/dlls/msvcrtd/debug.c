/*
 * msvcrtd.dll debugging code
 *
 * Copyright (C) 2003 Adam Gundy
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "wine/debug.h"

#include "winbase.h"

#define  _DEBUG
#include "crtdbg.h"

WINE_DEFAULT_DEBUG_CHANNEL(msvcrt);

int _crtAssertBusy = -1;
int _crtBreakAlloc = -1;
int _crtDbgFlag = 0;

#ifdef _WIN64
typedef unsigned __int64 MSVCRT_size_t;
#else
typedef unsigned long MSVCRT_size_t;
#endif

extern int _callnewh(MSVCRT_size_t);

/*********************************************************************
 *		??2@YAPAXIHPBDH@Z (MSVCRTD.@)
 */
void * CDECL MSVCRTD_operator_new_dbg(MSVCRT_size_t nSize, int nBlockUse,
                                      const char *szFileName, int nLine)
{
    void *retval = NULL;

    TRACE("(%lu, %d, '%s', %d)\n", nSize, nBlockUse, szFileName, nLine);

    switch(_BLOCK_TYPE(nBlockUse))
    {
    case _NORMAL_BLOCK:
        break;
    case _CLIENT_BLOCK:
        FIXME("Unimplemented case for nBlockUse = _CLIENT_BLOCK\n");
        return NULL;
    case _FREE_BLOCK:
        FIXME("Native code throws an exception here\n");
        return NULL;
    case _CRT_BLOCK:
    case _IGNORE_BLOCK:
        ERR("Not allowed nBlockUse value: %d\n", _BLOCK_TYPE(nBlockUse));
        return NULL;
    default:
        ERR("Unknown nBlockUse value: %d\n", _BLOCK_TYPE(nBlockUse));
        return NULL;
    }

    retval = HeapAlloc(GetProcessHeap(), 0, nSize);

    if (!retval)
        _callnewh(nSize);

    return retval;
}

/*********************************************************************
 *		_CrtSetDumpClient (MSVCRTD.@)
 */
void * CDECL _CrtSetDumpClient(void *dumpClient)
{
    return NULL;
}


/*********************************************************************
 *		_CrtSetReportHook (MSVCRTD.@)
 */
void * CDECL _CrtSetReportHook(void *reportHook)
{
    return NULL;
}


/*********************************************************************
 *		_CrtSetReportMode (MSVCRTD.@)
 */
int CDECL _CrtSetReportMode(int reportType, int reportMode)
{
    return 0;
}


/*********************************************************************
 *		_CrtSetBreakAlloc (MSVCRTD.@)
 */
int CDECL _CrtSetBreakAlloc(int new)
{
    int old = _crtBreakAlloc;
    _crtBreakAlloc = new;
    return old;
}

/*********************************************************************
 *		_CrtSetDbgFlag (MSVCRTD.@)
 */
int CDECL _CrtSetDbgFlag(int new)
{
    int old = _crtDbgFlag;
    _crtDbgFlag = new;
    return old;
}


/*********************************************************************
 *		_CrtDbgReport (MSVCRTD.@)
 */
int CDECL _CrtDbgReport(int reportType, const char *filename, int linenumber,
                        const char *moduleName, const char *format, ...)
{
    return 0;
}

/*********************************************************************
 *		_CrtDumpMemoryLeaks (MSVCRTD.@)
 */
int CDECL _CrtDumpMemoryLeaks(void)
{
    return 0;
}

/*********************************************************************
 *		_CrtCheckMemory (MSVCRTD.@)
 */
int CDECL _CrtCheckMemory(void)
{
    /* Note: maybe we could call here our heap validating functions ? */
    return TRUE;
}


/*********************************************************************
 *		__p__crtAssertBusy (MSVCRTD.@)
 */
int * CDECL __p__crtAssertBusy(void)
{
    return &_crtAssertBusy;
}

/*********************************************************************
 *		__p__crtBreakAlloc (MSVCRTD.@)
 */
int * CDECL __p__crtBreakAlloc(void)
{
    return &_crtBreakAlloc;
}

/*********************************************************************
 *		__p__crtDbgFlag (MSVCRTD.@)
 */
int * CDECL __p__crtDbgFlag(void)
{
    return &_crtDbgFlag;
}
