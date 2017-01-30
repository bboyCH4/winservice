#include <Shlwapi.h>
#include <Dbghelp.h>
#include <stdlib.h>
#include "CMinidump.h"


#pragma comment(lib,"Shlwapi.lib")
/* 函数说明：构造函数
*  输入参数：
*  输出参数：
*  返回类型：
*  说明：
*/
Minidump::WinMiniDump::WinMiniDump()
{
	m_preIph = NULL;
	m_prePch = NULL;
	m_logFunc = NULL;
}

Minidump::WinMiniDump::WinMiniDump(MiniDumpLog logFunc)
{
	m_preIph = NULL;
	m_prePch = NULL;
	m_logFunc = logFunc;
	SetInvalidHandle();
	Run();
}
/* 函数说明：析构函数
*  输入参数：
*  输出参数：
*  返回类型：
*  说明：
*/
Minidump::WinMiniDump::~WinMiniDump()
{
	UnSetInvalidHandle();
	Stop();
}
/* 函数说明：设置两个SEH异常捕获范畴外的异常
*  输入参数：
*  输出参数：
*  返回类型：
*  说明：
*/
void Minidump::WinMiniDump::SetInvalidHandle()
{
#if _MSC_VER >= 1400  // MSVC 2005/8
	m_preIph = _set_invalid_parameter_handler(InvalidParameterHandler);
#endif  // _MSC_VER >= 1400
	m_prePch = _set_purecall_handler(PureCallHandler);
}

/* 函数说明：非法参数错误调用处理
*  输入参数：
*  输出参数：
*  返回类型：
*  说明：
*/
void Minidump::WinMiniDump::InvalidParameterHandler(const wchar_t* expression,
													const wchar_t* function,
													const wchar_t* file,
													unsigned int line,
													uintptr_t pReserved)
{
	throw std::exception("Invalid Parameter");
}
/* 函数说明：纯虚函数错误调用处理
*  输入参数：
*  输出参数：
*  返回类型：
*  说明：
*/
void Minidump::WinMiniDump::PureCallHandler(void)
{
	//TODO:增加处理
	throw std::exception("Invalid Parameter");
}
/* 函数说明：回退到先前系统默认的异常处理
*  输入参数：
*  输出参数：
*  返回类型：
*  说明：
*/
void Minidump::WinMiniDump::UnSetInvalidHandle()
{
#if _MSC_VER >= 1400  // MSVC 2005/8
	_set_invalid_parameter_handler(m_preIph);
#endif  // _MSC_VER >= 1400
	_set_purecall_handler(m_prePch); //At application this can stop show the error message box.
}


LONG WINAPI  Minidump::WinMiniDump::UnhandledExceptionFilterEx(PEXCEPTION_POINTERS pException)
{
	char szDumpFilePath[MAX_PATH] = {0};
	char szExcutePath[MAX_PATH] = { 0 };			//执行文件所在路径
	char szExcuteName[MAX_PATH] = { 0 };			//执行文件名称
   
	GetModuleFileNameA(NULL, szExcutePath, MAX_PATH);
	_splitpath_s(szExcutePath, NULL, 0, NULL, 0, szExcuteName, MAX_PATH, NULL, 0);
	PathRemoveFileSpecA(szExcutePath);
	sprintf_s(szDumpFilePath, MAX_PATH,"%s\\%s.dmp",szExcutePath,szExcuteName);
	// 创建Dump文件  
	HANDLE hDumpFile = CreateFileA(szDumpFilePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hDumpFile == INVALID_HANDLE_VALUE)
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}
	// Dump信息  
	//  
	MINIDUMP_EXCEPTION_INFORMATION dumpInfo;
	dumpInfo.ExceptionPointers = pException;
	dumpInfo.ThreadId = GetCurrentThreadId();
	dumpInfo.ClientPointers = TRUE;
	// 写入Dump文件内容  
	//  
	if (FALSE == MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, MiniDumpNormal, &dumpInfo, NULL, NULL))
	{
		CloseHandle(hDumpFile);
		return EXCEPTION_CONTINUE_SEARCH;
	}
	CloseHandle(hDumpFile);
	return EXCEPTION_EXECUTE_HANDLER;
}


VOID  Minidump::WinMiniDump::MiniLog(const char * f, ...)
{
	if (m_logFunc)
	{
		va_list m;
		va_start(m, f);
		m_logFunc(f, m);
		va_end(m);
	}
}

VOID Minidump::WinMiniDump::Run()
{
	m_preFilter = ::SetUnhandledExceptionFilter(UnhandledExceptionFilterEx);
	PreventSetUnhandledExceptionFilter();
}

BOOL Minidump::WinMiniDump::PreventSetUnhandledExceptionFilter()
{
	HMODULE hKernel32 = LoadLibrary(L"kernel32.dll");
	if (hKernel32 ==   NULL)
	{
		return FALSE;
	}
	void *pOrgEntry = ::GetProcAddress(hKernel32, "SetUnhandledExceptionFilter");
	if(pOrgEntry == NULL)
	{
		return FALSE;
	}

	unsigned char newJump[5];
	DWORD dwOrgEntryAddr = (DWORD)pOrgEntry;
	dwOrgEntryAddr += 5; //jump instruction has 5 byte space.

	void *pNewFunc = &TempSetUnhandledExceptionFilter;
	DWORD dwNewEntryAddr = (DWORD)pNewFunc;
	DWORD dwRelativeAddr = dwNewEntryAddr - dwOrgEntryAddr;

	newJump[0] = 0xE9;  //jump
	memcpy(&newJump[1], &dwRelativeAddr, sizeof(DWORD));
	SIZE_T bytesWritten;
	DWORD dwOldFlag, dwTempFlag;
	::VirtualProtect(pOrgEntry, 5, PAGE_READWRITE, &dwOldFlag);
	BOOL bRet = ::WriteProcessMemory(::GetCurrentProcess(), pOrgEntry, newJump, 5, &bytesWritten);
	::VirtualProtect(pOrgEntry, 5, dwOldFlag, &dwTempFlag);
	return bRet;
}

LPTOP_LEVEL_EXCEPTION_FILTER WINAPI Minidump::WinMiniDump::TempSetUnhandledExceptionFilter( LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter )
{
	return NULL;
}

VOID Minidump::WinMiniDump::Stop()
{
	//x64 will crash
	//if(m_preFilter != NULL)
	//{
	//	::SetUnhandledExceptionFilter(m_preFilter);
	//	m_preFilter = NULL;
	//}
	return ;
}