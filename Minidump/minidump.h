/*
 *由于采用动态加载，为了保证导出的函数名和预期一致
 *应当使用def文件
*/
#ifndef _MINIDUMP_H_
#define _MINIDUMP_H_
#include <windows.h>
/*------------------------------------- 类型定义 --------------------------------------*/
//调用规定
#	define _MINIDUMP_CALLCONV __stdcall


typedef  void  (* MiniDumpLog)(const char * f, ...);


//本地工程必须有相应的v4vSession.cpp才得行，才有exports
//链接符号
#ifdef MINIDUMP_EXPORTS
#	define _MINIDUMP_API __declspec(dllexport)
#else 
#	define _MINIDUMP_API __declspec(dllimport)
#endif 

/*==================================== 类型定义 ======================================*/



/*------------------------------------- 对外接口 --------------------------------------*/
#ifdef __cplusplus
extern "C"{
#endif // __cplusplus
_MINIDUMP_API BOOL  _MINIDUMP_CALLCONV StartCrashDump(MiniDumpLog func);
_MINIDUMP_API void  _MINIDUMP_CALLCONV StopCrashDump();
#ifdef __cplusplus
} // Ends extern "C"
#endif // __cplusplus
/*==================================== 对外接口 ======================================*/






#endif
