/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <iostream>
#include <tchar.h>

LPCTSTR g_sComPortName = NULL;
LPCTSTR g_sFileName = NULL;
LPCTSTR g_sCommDcbStr = NULL;

//000000  000240         NOP               ; Опознавательный знак
//000002  000447         BR      000122    ; Запуск загрузчика

////000106  012706 001000  MOV     #001000, SP
////000112  012705 000414  MOV     #000414, R5
////000116  004737 165402  CALL    165402$         ; Вывести строку на экран
//000122  012701 001000  MOV   #001000,R1  ; Адрес куда считывать остаток файла
//000126  013702 000324  MOV   @#000324,R2 ; Длина остатка в словах
//000132  006302         ASL   R2          ; Длина остатка в байтах
//000134  105737 176570  TSTB  @#176570    ; Приемник готов ?
//000140  100375         BPL   000132      ; Нет
//000142  113721 176572  MOVB  @#176572,(R1)+  ; Переслать принятый байт в память
//000146  077206         SOB   R2,000132   ; Продолжаем пока не прочитали всё
//000150  013706 000042  MOV   @#000042,SP
//000154  013707 000040  MOV   @#000040,PC ; Запускаем загруженную программу

static WORD const loader1[] = {
/*000*/ 000240, 000447, 000000, 000000, 000000, 000000, 000000, 000000, 
/*020*/ 000000, 000000, 000000, 000000, 000000, 000000, 000000, 000000, 
/*040*/ 000000, 000000, 000000, 000000, 000000, 000000, 000000, 000000, 
/*060*/ 000000, 000000, 000000, 000000, 000000, 000000, 000000, 000000, 
/*100*/ 000000, 000000, 000000, 000240, 000240, 000240, 000240, 000240, 
/*120*/ 000240, 012701, 001000, 013702, 000324, 006302,0105737,0176570,
/*140*/0100375,0113721,0176572, 077206, 013706, 000042, 013707, 000040, 
/*160*/ 000000, 000000, 000000, 000000, 000000, 000000, 000000, 000000, 
/*200*/ 000000, 000000, 000000, 000000, 000000, 000000, 000000, 000000, 
/*220*/ 000000, 000000, 000000, 000000, 000000, 000000, 000000, 000000, 
/*240*/ 000000, 000000, 000000, 000000, 000000, 000000, 000000, 000000, 
/*260*/ 000000, 000000, 000000, 000000, 000000, 000000, 000000, 000000, 
/*300*/ 000000, 000000, 000000, 000000, 000000, 000000, 000000, 000000, 
/*320*/ 000000, 000000, 000000, 000000, 000000, 000000, 000000, 000000, 
/*340*/ 000000, 000000, 000000, 000000, 000000, 000000, 000000, 000000, 
/*360*/ 000000, 000000, 000000, 000000, 000000, 000000, 000000, 000000, 
/*400*/ 000000, 000000, 000000, 000000, 000000, 000000, 067514, 062141, 
/*420*/ 067151, 020147, 000000, 000000, 000000, 000000, 000000, 000000, 
/*440*/ 000000, 000000, 000000, 000000, 000000, 000000, 000000, 000000, 
/*460*/ 000000, 000000, 000000, 000000, 000000, 000000, 000000, 000000, 
};

bool ParseCommandLine(int argc, TCHAR* argv[])
{
    for (int argn = 1; argn < argc; argn++)
    {
        LPCTSTR arg = argv[argn];
        if (arg[0] == _T('-') || arg[0] == _T('/'))
        {
            if (_wcsicmp(arg + 1, _T("dcb")) == 0)
            {
                argn++;
                if (argn == argc)
                {
                    wprintf(_T("Missing parameter for option -dcb.\n"));
                    return false;
                }
                g_sCommDcbStr = argv[argn];
            }
            else
            {
                wprintf(_T("Unknown option: %s\n"), arg);
                return false;
            }
        }
        else
        {
            if (g_sComPortName == NULL)
                g_sComPortName = arg;
            else if (g_sFileName == NULL)
                g_sFileName = arg;
            else
            {
                wprintf(_T("Unknown param: %s\n"), arg);
                return false;
            }
        }
    }

    // Parsed options validation
    if (g_sComPortName == NULL)
    {
        wprintf(_T("Serial port name is not specified.\n"));
        return false;
    }
    if (g_sFileName == NULL)
    {
        wprintf(_T("File name is not specified.\n"));
        return false;
    }

    return true;
}

int _tmain(int argc, TCHAR* argv[])
{
    wprintf(_T("UkncComSender Utility  v1.2  by Nikita Zimin  [%S %S]\n\n"), __DATE__, __TIME__);

    if (!ParseCommandLine(argc, argv))
    {
        wprintf(
            _T("\n")
            _T("Usage: UkncComSender [<Options>] <PortName> <FileToSendName>\n")
            _T("Options:\n")
            _T("  -dcb \"<DcbParams>\"\n")
            _T("<DcbParams>:\n")
            _T("  baud=b\n")
            _T("  parity={n|e|o|m|s}\n")
            _T("  data={5|6|7|8}\n")
            _T("  stop={1|1.5|2}\n")
            _T("  to={on|off}\n")
            _T("  xon={on|off}\n")
            _T("  odsr={on|off}\n")
            _T("  octs={on|off}\n")
            _T("  dtr={on|off|hs}\n")
            _T("  rts={on|off|hs|tg}\n")
            _T("  idsr={on|off}\n")
            _T("\n"));
        return -1;
    }

    TCHAR portname[32];
    wsprintf(portname, _T("\\\\.\\%s"), g_sComPortName);

    HANDLE hComPort = ::CreateFile(portname,
        GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
        0, NULL);
    if (hComPort == INVALID_HANDLE_VALUE)
    {
        DWORD dwError = ::GetLastError();
        wprintf(_T("Failed to open serial port %s (0x%08lx).\n"), g_sComPortName, dwError);
        return FALSE;
    }
    wprintf(_T("Serial port %s opened.\n"), g_sComPortName);

    // Prepare port settings
    DCB dcb;
    ::memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    dcb.BaudRate = 9600;
    dcb.ByteSize = 8;
    dcb.fBinary = 1;
    dcb.fParity = FALSE;
    dcb.fOutxCtsFlow = dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fTXContinueOnXoff = FALSE;
    dcb.fOutX = dcb.fInX = FALSE;
    dcb.fErrorChar = FALSE;
    dcb.fNull = FALSE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fAbortOnError = FALSE;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    if (!::BuildCommDCB(g_sCommDcbStr, &dcb))
    {
        ::CloseHandle(hComPort);
        hComPort = INVALID_HANDLE_VALUE;
        DWORD dwError = ::GetLastError();
        wprintf(_T("Failed to parse port configuration string \"%s\" (0x%08lx).\n"), g_sCommDcbStr, dwError);
        return FALSE;
    }
    if (!::SetCommState(hComPort, &dcb))
    {
        ::CloseHandle(hComPort);
        hComPort = INVALID_HANDLE_VALUE;
        DWORD dwError = ::GetLastError();
        wprintf(_T("Failed to configure port %s (0x%08lx).\n"), g_sComPortName, dwError);
        return FALSE;
    }

    // Set timeouts: ReadIntervalTimeout value of MAXDWORD, combined with zero values for both the ReadTotalTimeoutConstant
    // and ReadTotalTimeoutMultiplier members, specifies that the read operation is to return immediately with the bytes
    // that have already been received, even if no bytes have been received.
    COMMTIMEOUTS timeouts;
    ::memset(&timeouts, 0, sizeof(timeouts));
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 100;
    if (!::SetCommTimeouts(hComPort, &timeouts))
    {
        ::CloseHandle(hComPort);
        hComPort = INVALID_HANDLE_VALUE;
        DWORD dwError = ::GetLastError();
        wprintf(_T("Failed to set the COM port timeouts (0x%08lx).\n"), dwError);
        return FALSE;
    }

    // Clear port input buffer
    ::PurgeComm(hComPort, PURGE_RXABORT|PURGE_RXCLEAR);

    wprintf(_T("Serial port configured.\n"));

    // Open the input file
    wprintf(_T("Opening the input file %s...\n"), g_sFileName);
    FILE* fpfile = ::_wfopen(g_sFileName, _T("rb"));
    if (fpfile == 0)
    {
        ::CloseHandle(hComPort);
        hComPort = INVALID_HANDLE_VALUE;
        wprintf(_T("Failed to open file %s for reading.\n"), g_sFileName);
        return -1;
    }

    //TODO: Get file size

    wprintf(_T("Reading the first block...\n"));
    BYTE buffer[512];
    size_t lBytesRead = ::fread(buffer, 1, 512, fpfile);
    //TODO: Check for error

    // Prepare the loader
    //const char filename[] = "SAMPLE";
    WORD i;
    WORD * wbuffer = (WORD*) buffer;
    wbuffer[0] = loader1[0];
    wbuffer[1] = loader1[1];
    for (i = 0100/2; i < 0200/2; ++i)  wbuffer[i] = loader1[i];
    //size_t filenamelen = strlen(filename);
    //::memcpy(buffer + 0302, filename, filenamelen);
    //::memset(buffer + 0302 + filenamelen, 0, 16 - filenamelen);
    //::memcpy(buffer + 0424, filename, filenamelen);
    //*(buffer + 0424 + filenamelen + 0) = 13;
    //*(buffer + 0424 + filenamelen + 1) = 10;
    //*(buffer + 0424 + filenamelen + 2) = 0;
    WORD datalen = *(wbuffer + 050/2);
    //wprintf(_T("datalen %06o 0x%04x\n"), datalen, datalen);
    *(wbuffer + 0324/2) = (*(wbuffer + 050/2) - 0776) / 2;

    wprintf(_T("Waiting for byte 0100...\n"));
    while (true)
    {
        BYTE b;
        DWORD dwBytesRead = 0;
        if (!::ReadFile(hComPort, &b, 1, &dwBytesRead, NULL))
            continue;
        if (dwBytesRead <= 0) continue;
        wprintf(_T("0x%02x\n"), (int)b);
        if (b == 0100) break;
    }

    wprintf(_T("Sending loader...\n"));
    for (int i = 0; i < 512; i++)
    {
        DWORD dwBytesWritten;
        ::WriteFile(hComPort, buffer + i, 1, &dwBytesWritten, NULL);
    }

    // Send the remaining bytes
    WORD remlen = datalen + 2 - 01000;
    //WORD sent = 0;
    //wprintf(_T("Sending data 0x%04x "), remlen);
    wprintf(_T("Sending data "));
    while (remlen > 0)
    {
        int blocklen = (remlen >= 512) ? 512 : remlen;
        lBytesRead = ::fread(buffer, 1, blocklen, fpfile);
        //TODO: Check for error
        for (int i = 0; i < blocklen; i++)
        {
            DWORD dwBytesWritten;
            ::WriteFile(hComPort, buffer + i, 1, &dwBytesWritten, NULL);
            //sent++;
        }

        remlen -= blocklen;
        wprintf(_T("."));
    }
    //wprintf(_T(" sent 0x%04x\n"), sent);
    wprintf(_T("\n"));

    ::fclose(fpfile);

    ::CloseHandle(hComPort);
    wprintf(_T("COM port closed.\n"));
}
