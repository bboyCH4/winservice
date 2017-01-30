#include "CMinidump.h"
#include "minidump.h"

Minidump::WinMiniDump* _pWinDump = NULL;


/* 函数说明：Dll入口函数
*  输入参数：
*  输出参数：
*  返回类型：
*  说明：
*/
BOOL WINAPI DllMain(
	_In_ HINSTANCE hinstDLL,
	_In_ DWORD     fdwReason,
	_In_ LPVOID    lpvReserved
	)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		break;
	case DLL_PROCESS_DETACH:
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	}
	return TRUE;
}

 BOOL  _MINIDUMP_CALLCONV StartCrashDump(MiniDumpLog func)
 {
	 if (_pWinDump == NULL && func)
	 {
		 _pWinDump =  new Minidump::WinMiniDump(func);
		 return TRUE;
	 }
	 return FALSE;
 }
 void  _MINIDUMP_CALLCONV StopCrashDump()
 {
	 if (_pWinDump)
		 delete _pWinDump;
 }