/*
    Copyright (C) 2004,2020 Aleksey Kochetov

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "StdAfx.h"
#include <sstream>
#include <iomanip>
#include <process.h>
#if !defined(_WIN64)
#include <imagehlp.h>
#else
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif
#include "StackTracer.h"

#define MAX_NAME_LENGTH 1024

namespace Common {

    using namespace std;

    static const char* failure = "StackTracer::trace failed.";

#if !defined(_WIN64)
void StackTracer::Trace (string& result, EXCEPTION_POINTERS* exInfo)
{
    std::ostringstream out;
    out.setf(ios_base::showbase);
    out.unsetf(ios_base::dec);
    out.setf(ios_base::hex);

    HANDLE hProcess =::GetCurrentProcess();
    HANDLE hThread = ::GetCurrentThread();

    STACKFRAME stackFrame;
    ZeroMemory(&stackFrame, sizeof(stackFrame));
    stackFrame.AddrPC.Mode      = AddrModeFlat;
    stackFrame.AddrStack.Mode   = AddrModeFlat;
    stackFrame.AddrFrame.Mode   = AddrModeFlat;

    if (exInfo)
    {
        stackFrame.AddrPC.Offset    = exInfo->ContextRecord->Eip;
        stackFrame.AddrStack.Offset = exInfo->ContextRecord->Esp;
        stackFrame.AddrFrame.Offset = exInfo->ContextRecord->Ebp;
    }
    else
    {
        CONTEXT context;
        ZeroMemory(&context, sizeof(context));
        context.ContextFlags = CONTEXT_FULL;

        __asm
        {
        Label:
          mov [context.Ebp], ebp;
          mov [context.Esp], esp;
          mov eax, [Label];
          mov [context.Eip], eax;
        }

        stackFrame.AddrPC.Offset    = context.Eip;
        stackFrame.AddrStack.Offset = context.Esp;
        stackFrame.AddrFrame.Offset = context.Ebp;
    }

    if (::SymInitialize(hProcess, NULL, TRUE))
    {
        while (::StackWalk(IMAGE_FILE_MACHINE_I386, hProcess, hThread,
                            &stackFrame, NULL, NULL,
                            SymFunctionTableAccess, SymGetModuleBase, NULL)
            && stackFrame.AddrFrame.Offset)
        {
            out << stackFrame.AddrPC.Offset << '\t';

            IMAGEHLP_MODULE moduleInfo;
            memset(&moduleInfo, 0, sizeof(moduleInfo) );
            moduleInfo.SizeOfStruct = sizeof(moduleInfo);

            DWORD dwModBase = SymGetModuleBase(hProcess, stackFrame.AddrPC.Offset);
            if (dwModBase && SymGetModuleInfo(hProcess, stackFrame.AddrPC.Offset, &moduleInfo))
                out << '\t' << moduleInfo.ModuleName;

            char buff[sizeof(IMAGEHLP_SYMBOL) + MAX_NAME_LENGTH];
            ZeroMemory(&buff, sizeof(buff));
            PIMAGEHLP_SYMBOL pSym = (PIMAGEHLP_SYMBOL)&buff;
            pSym->SizeOfStruct  = sizeof(IMAGEHLP_SYMBOL) + MAX_NAME_LENGTH;
            pSym->MaxNameLength = MAX_NAME_LENGTH;

            DWORD displacement;
            if (::SymGetSymFromAddr(hProcess, stackFrame.AddrPC.Offset, &displacement, pSym))
                out << '\t' << pSym->Name << "() + " << displacement;
            else if (dwModBase)
                out << '\t' << stackFrame.AddrPC.Offset - dwModBase;

            out << endl;
        }
        result = out.str();

        ::SymCleanup(hProcess);
    }
    else
        result = failure;
}
#else
void StackTracer::Trace (string& result, EXCEPTION_POINTERS* exInfo)
{
    std::ostringstream out;
    out.setf(ios_base::showbase);
    out.unsetf(ios_base::dec);
    out.setf(ios_base::hex);

    HANDLE hProcess = ::GetCurrentProcess();
    HANDLE hThread = ::GetCurrentThread();

    CONTEXT context;
    if (exInfo && exInfo->ContextRecord) {
        context = *exInfo->ContextRecord;
    } else {
        RtlCaptureContext(&context);
    }

    STACKFRAME64 stackFrame;
    ZeroMemory(&stackFrame, sizeof(stackFrame));
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrStack.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrPC.Offset = context.Rip;
    stackFrame.AddrStack.Offset = context.Rsp;
    stackFrame.AddrFrame.Offset = context.Rbp;

    if (::SymInitialize(hProcess, NULL, TRUE))
    {
        DWORD64 displacement = 0;
        char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_NAME_LENGTH];
        PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)symbolBuffer;
        pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        pSymbol->MaxNameLen = MAX_NAME_LENGTH;

        while (::StackWalk64(
            IMAGE_FILE_MACHINE_AMD64,
            hProcess,
            hThread,
            &stackFrame,
            &context,
            NULL,
            SymFunctionTableAccess64,
            SymGetModuleBase64,
            NULL)
            && stackFrame.AddrPC.Offset)
        {
            out << stackFrame.AddrPC.Offset;

            DWORD64 modBase = SymGetModuleBase64(hProcess, stackFrame.AddrPC.Offset);
            if (modBase) {
                IMAGEHLP_MODULE64 moduleInfo;
                memset(&moduleInfo, 0, sizeof(moduleInfo));
                moduleInfo.SizeOfStruct = sizeof(moduleInfo);
                if (SymGetModuleInfo64(hProcess, stackFrame.AddrPC.Offset, &moduleInfo)) {
                    out << '\t' << moduleInfo.ModuleName;
                }
            }

            if (SymFromAddr(hProcess, stackFrame.AddrPC.Offset, &displacement, pSymbol)) {
                out << '\t' << pSymbol->Name << "() + " << displacement;
            }
            out << std::endl;
        }
        result = out.str();
        ::SymCleanup(hProcess);
    }
    else {
        result = failure;
    }
}
#endif // !defined(_WIN64)

}//namespace Common
