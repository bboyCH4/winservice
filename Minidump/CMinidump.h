#include <windows.h>
#include <stdlib.h>
#include "WinExcepeion.h"



namespace Minidump
{
	class WinMiniDump{
	#define		MOUDLENAME	"WinMiniDump"
	#define		DEFAULT_SAVE_PATH "C:\\logs"
		//tofix:���д�꺯��
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
		
		//���õ�handler����Ϊstatic�ģ�����Ϊ�����������
		//SEH֮����쳣
		static void InvalidParameterHandler(const wchar_t* expression, 
											const wchar_t* function, 
											const wchar_t* file,
											unsigned int line,
											uintptr_t pReserved);
		static void PureCallHandler(void);
		//SEH֮����쳣
		static LONG WINAPI UnhandledExceptionFilterEx(PEXCEPTION_POINTERS pException);
		static LPTOP_LEVEL_EXCEPTION_FILTER WINAPI Minidump::WinMiniDump::TempSetUnhandledExceptionFilter( LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter );
		
		void SetInvalidHandle();
		void UnSetInvalidHandle();
		VOID MiniLog(const char * f, ...);
		VOID Run();
		VOID Stop();
		BOOL PreventSetUnhandledExceptionFilter();
	private:
		_invalid_parameter_handler		m_preIph;					//�����滻ǰ�Ĵ���Ƿ����������handler
		_purecall_handler				m_prePch;					//�����滻ǰ�Ĵ��麯���Ƿ�����handler
		MiniDumpLog						m_logFunc;					//��־����
		LPTOP_LEVEL_EXCEPTION_FILTER	m_preFilter;				//ǰһ��excepetion filter

	};
}