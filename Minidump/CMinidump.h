#include <windows.h>
#include <stdlib.h>
#include "WinExcepeion.h"



namespace Minidump
{
	class WinMiniDump{
	#define		MOUDLENAME	"WinMiniDump"
	#define		DEFAULT_SAVE_PATH "C:\\logs"
		//tofix:如何写宏函数
	//#define Log(...) ({ \
	//	if (m_logFunc != NULL)						\
	//	{											\
	//		m_logFunc(## __VA_ARGS__)				\
	//	}											\
	//	else{										\
	//					;							\
	//	}											\
	//})
	typedef  void  (* MiniDumpLog)(const char * f, ...);
	public:
		WinMiniDump();
		WinMiniDump(MiniDumpLog logFunc);
		~WinMiniDump();
	private:
		
		//设置的handler必须为static的，不能为具体对象所有
		//SEH之外的异常
		static void InvalidParameterHandler(const wchar_t* expression, 
											const wchar_t* function, 
											const wchar_t* file,
											unsigned int line,
											uintptr_t pReserved);
		static void PureCallHandler(void);
		//SEH之类的异常
		static LONG WINAPI UnhandledExceptionFilterEx(PEXCEPTION_POINTERS pException);
		static LPTOP_LEVEL_EXCEPTION_FILTER WINAPI Minidump::WinMiniDump::TempSetUnhandledExceptionFilter( LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter );
		
		void SetInvalidHandle();
		void UnSetInvalidHandle();
		VOID MiniLog(const char * f, ...);
		VOID Run();
		VOID Stop();
		BOOL PreventSetUnhandledExceptionFilter();
	private:
		_invalid_parameter_handler		m_preIph;					//保存替换前的处理非法输入参数的handler
		_purecall_handler				m_prePch;					//保存替换前的纯虚函数非法调用handler
		MiniDumpLog						m_logFunc;					//日志函数
		LPTOP_LEVEL_EXCEPTION_FILTER	m_preFilter;				//前一个excepetion filter

	};
}