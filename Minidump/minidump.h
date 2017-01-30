/*
 *���ڲ��ö�̬���أ�Ϊ�˱�֤�����ĺ�������Ԥ��һ��
 *Ӧ��ʹ��def�ļ�
*/
#ifndef _MINIDUMP_H_
#define _MINIDUMP_H_
#include <windows.h>
/*------------------------------------- ���Ͷ��� --------------------------------------*/
//���ù涨
#	define _MINIDUMP_CALLCONV __stdcall


typedef  void  (* MiniDumpLog)(const char * f, ...);


//���ع��̱�������Ӧ��v4vSession.cpp�ŵ��У�����exports
//���ӷ���
#ifdef MINIDUMP_EXPORTS
#	define _MINIDUMP_API __declspec(dllexport)
#else 
#	define _MINIDUMP_API __declspec(dllimport)
#endif 

/*==================================== ���Ͷ��� ======================================*/



/*------------------------------------- ����ӿ� --------------------------------------*/
#ifdef __cplusplus
extern "C"{
#endif // __cplusplus
_MINIDUMP_API BOOL  _MINIDUMP_CALLCONV StartCrashDump(MiniDumpLog func);
_MINIDUMP_API void  _MINIDUMP_CALLCONV StopCrashDump();
#ifdef __cplusplus
} // Ends extern "C"
#endif // __cplusplus
/*==================================== ����ӿ� ======================================*/






#endif
