#include <windows.h>
#include <Wtsapi32.h>
#include <Userenv.h>
#include "LogModule.h"

static void ServiceUninstall();
static void ServiceMain(int argc, char** argv);
static void ServiceControlHandler(DWORD request);
static void ServiceControlManagerUpdate(DWORD dwExitCode, DWORD dwState);
static BOOL ServiceInit();
static void LaunchAppInSession(LPSTR lpCommand);

static void AddFeature(struct watch_feature_set* wfs, 
						const char *path, 
						const char *flag, 
						const char *name,
						void(*handler)(void *), 
						void *ctx);

static SERVICE_STATUS				_ServiceStatus;
static SERVICE_STATUS_HANDLE		_hStatus;
static HANDLE						_hServiceExitEvent;




struct watch_feature 
{
	struct watch_event *watch;
	const char *feature_flag;
	const char *name;
	void(*handler)(void *);
	void *ctx;
};

#define MAX_FEATURES 10
struct watch_feature_set 
{
	struct watch_feature features[MAX_FEATURES];
	unsigned nr_features;							//当前feature个数
};



#define SVC_NAME "vTopSrv"
#define SVC_DISPLAYNAME "Halsign Tools for Virtual Machines Service"
void main(int argn, char *argv[])
{

	if (LoadLogModule("vTopGuestAgent"))
	{
		return ;
	}
	InfoLog(__FUNCTION__ " begin");
	if (argn == 1)
	{
		//一个程序可能包含若干个服务。
		//每一个服务都必须列于专门的分派表SERVICE_TABLE_ENTRY
		SERVICE_TABLE_ENTRYA ServiceTable[2];
		//指向表示服务名称字符串的指针
		ServiceTable[0].lpServiceName = SVC_NAME;
		//指向服务主函数的指针
		ServiceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTIONA)ServiceMain;
		//分派表的最后一项必须是服务名和服务主函数域的 NULL 指针
		//文本例子程序中只宿主一个服务，所以服务名的定义是可选的。
		ServiceTable[1].lpServiceName = NULL;
		ServiceTable[1].lpServiceProc = NULL;
		//SCM 启动某个服务时，它等待某个进程的主线程来调用 StartServiceCtrlDispatcher 函数
		//将分派表传递给 StartServiceCtrlDispatcher。
		//这将把调用进程的主线程转换为控制分派器。
		//该分派器启动一个新线程，该线程运行分派表中每个服务的 ServiceMain 函数
		// Start the control dispatcher thread for our service
		if (!StartServiceCtrlDispatcherA(ServiceTable))
		{
			if (GetLastError() == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
			{
				ErrorLog("XenSvc: unable to start ctrl dispatcher - %d", GetLastError());
			}
		}
		else
		{
			// We get here when the service is shut down.
			ErrorLog(" service is shut down");
		}
	}
	else if (!lstrcmpiA(argv[1], "-u"))
	{
		ServiceUninstall();
	}
	else
	{
		WarnLog("Invalid input parameters");
	}
	InfoLog(__FUNCTION__ " end");
	UnloadLogModule();
}


void ServiceUninstall()
{
	SC_HANDLE			hSvc;
	SC_HANDLE			hMgr;

	hMgr = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);

	if (hMgr)
	{
		hSvc = OpenServiceA(hMgr, SVC_NAME, SERVICE_ALL_ACCESS);
		
		if (hSvc)
		{
			// Sends a control code to a service.
			if (ControlService(hSvc, SERVICE_CONTROL_STOP, &_ServiceStatus))
			{
				InfoLog("Stopping %s", SVC_DISPLAYNAME);
				Sleep(1000);

				while (QueryServiceStatus(hSvc, &_ServiceStatus))
				{
					//正在stop就再等等
					if (_ServiceStatus.dwCurrentState == SERVICE_STOP_PENDING)
					{
						InfoLog("Stop pending");
						Sleep(1000);
					}
					else
						break;
				}

				if (_ServiceStatus.dwCurrentState == SERVICE_STOPPED)
					InfoLog("%s stopped", SVC_DISPLAYNAME);
				else
					InfoLog("%s failed to stop", SVC_DISPLAYNAME);
			}//end ControlService SERVICE_CONTROL_STOP
			// now remove the service
			if (DeleteService(hSvc))
				InfoLog("%s uninstalled", SVC_DISPLAYNAME);
			else
				InfoLog("Unable to uninstall - %d", GetLastError());
			CloseServiceHandle(hSvc);
			/* Tell dom0 that we're no longer installed.  This is a bit
			of a hack. */
		}
		else
		{
			ErrorLog("OpenServiceA failed with ErrorCode %d", GetLastError());
		}
		CloseServiceHandle(hMgr);
	}
	else
	{
		ErrorLog("OpenSCManagerA failed with ErrorCode %d",GetLastError());
	}
}

//SC启动服务后，进入该service main
void ServiceMain(int argc, char** argv)
{
	InfoLog(__FUNCTION__" :start");

	_hServiceExitEvent = CreateEvent(NULL, false, false, NULL);
	
	if (_hServiceExitEvent == NULL)
	{
		ErrorLog("XenSvc: Unable to create the event obj - %d", GetLastError());
		return;
	}

	if (!ServiceInit())
	{
		ErrorLog("XenSvc: Unable to init xenservice");
		return;
	}
	LaunchAppInSession("notepad");
	WaitForSingleObject(_hServiceExitEvent, INFINITE);
	ServiceControlManagerUpdate(0, SERVICE_STOPPED);
	CloseHandle(_hServiceExitEvent);
	InfoLog(__FUNCTION__" :end");
	return;
}

BOOL ServiceInit()
{
	_ServiceStatus.dwServiceType = SERVICE_WIN32;
	_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
	_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	//The error code the service uses to report an error that occurs
	//when it is starting or stopping
	_ServiceStatus.dwWin32ExitCode = 0;
	//A service-specific error code that the service returns 
	//when an error occurs while the service is starting or stopping
	_ServiceStatus.dwServiceSpecificExitCode = 0;
	// service increments periodically to report its progress 
	//during a lengthy start, stop, pause, or continue operation
	_ServiceStatus.dwCheckPoint = 0;
	//The estimated time required for a pending start, stop, pause,
	//or continue operation, in milliseconds. 
	//Before the specified amount of time has elapsed, 
	//the service should make its next call to the SetServiceStatus function with
	//either an incremented dwCheckPoint value or a change in dwCurrentState.
	_ServiceStatus.dwWaitHint = 0;

	_hStatus = RegisterServiceCtrlHandlerA(SVC_NAME, (LPHANDLER_FUNCTION)ServiceControlHandler);

	if (_hStatus == (SERVICE_STATUS_HANDLE)0)
	{
		// Registering Control Handler failed
		ErrorLog("XenSvc: Registering service control handler failed - %d", GetLastError());
		return false;
	}

	_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
	SetServiceStatus(_hStatus, &_ServiceStatus);

	return true;
}


// Service control handler function
void ServiceControlHandler(DWORD request)
{
	switch (request)
	{
	case SERVICE_CONTROL_STOP:
		InfoLog("XenSvc: xenservice stopped");
		ServiceControlManagerUpdate(0, SERVICE_STOP_PENDING);
		SetEvent(_hServiceExitEvent);
		return;

	case SERVICE_CONTROL_SHUTDOWN:
		InfoLog("XenSvc: xenservice shutdown");
		ServiceControlManagerUpdate(0, SERVICE_STOP_PENDING);
		SetEvent(_hServiceExitEvent);
		return;

	default:
		WarnLog("XenSvc: unknown request");
		break;
	}

	return;
}

void ServiceControlManagerUpdate(DWORD dwExitCode, DWORD dwState)
{
	_ServiceStatus.dwWin32ExitCode = dwExitCode;
	_ServiceStatus.dwCurrentState = dwState;
	SetServiceStatus(_hStatus, &_ServiceStatus);
}

void Run()
{
	STARTUPINFOA					startInfo;
	PROCESS_INFORMATION			processInfo;

	InfoLog(__FUNCTION__" Guest agent main loop starting");

	//GetWindowsVersion();

	ZeroMemory(&startInfo, sizeof(startInfo));
	ZeroMemory(&processInfo, sizeof(processInfo));
	startInfo.cb = sizeof(startInfo);

	//update performance information in the WMI repository
	//allows developers and IT Administrators to create scripts
	//that access performance information
	//
	// Refresh WMI ADAP classes
	//
	if (!CreateProcessA(
		NULL,
		"\"wmiadap\" /f",
		NULL,
		NULL,
		FALSE,
		0,
		NULL,
		NULL,
		&startInfo,
		&processInfo
		))
	{
		WarnLog("XenSvc: Unable to refresh WMI ADAP: %d", GetLastError());
	}
	else
	{
		//Wait until child process exits.
		WaitForSingleObject(processInfo.hProcess, 5000);

		CloseHandle(processInfo.hProcess);
		CloseHandle(processInfo.hThread);
	}

	//
	// Enable disk counters forcibly so that we can retrieve disk performance
	// using IOCTL_DISK_PERFORMANCE
	//
	if (!CreateProcessA(
		NULL,
		"\"diskperf\" -y",
		NULL,
		NULL,
		FALSE,
		0,
		NULL,
		NULL,
		&startInfo,
		&processInfo
		))
	{
		WarnLog("XenSvc: Cannot enable disk perf counters: %d", GetLastError());
	}
	else
	{
		WaitForSingleObject(processInfo.hProcess, 5000);

		CloseHandle(processInfo.hProcess);
		CloseHandle(processInfo.hThread);
	}


}

void AddFeature(struct watch_feature_set *wfs, 
				const char *path,
				const char *flag,
				const char *name,
				void(*handler)(void *), 
				void *ctx)
{
	unsigned n;
	if (wfs->nr_features == MAX_FEATURES) 
	{
		WarnLog("Too many features!", ERROR_INVALID_FUNCTION);
		return;
	}
	n = wfs->nr_features;
	//wfs->features[n].watch = EstablishWatch(path);
	
	if (wfs->features[n].watch == NULL) 
	{
		WarnLog("EstablishWatch() for AddFeature()");
		return;
	}
	wfs->features[n].feature_flag = flag;
	wfs->features[n].handler = handler;
	wfs->features[n].ctx = ctx;
	wfs->features[n].name = name;
	wfs->nr_features++;
}
 void LaunchAppInSession(LPSTR lpCommand)
{
	DWORD						dwSessionId = 0;
	HANDLE						hUserToken = NULL;
	PROCESS_INFORMATION			pi;
	STARTUPINFOA				si;
	DWORD						dwRet = 0;
	HANDLE						hPToken = NULL;
	HANDLE						hUserTokenDup = NULL;
	DWORD						dwCreationFlags;


	dwSessionId = WTSGetActiveConsoleSessionId();

	InfoLog(__FUNCTION__" Session Id %ul", dwSessionId);

	do 
	{
		LUID luid;
		TOKEN_PRIVILEGES tp;
		//
		//new process has a new console, instead of inheriting its parent's console (the default). 
		dwCreationFlags = NORMAL_PRIORITY_CLASS | CREATE_NEW_CONSOLE;

		WTSQueryUserToken(dwSessionId, &hUserToken);

		ZeroMemory(&si, sizeof(STARTUPINFO));
		si.cb = sizeof(STARTUPINFO);
		//The system  creates the default input desktop for the interactive window station(Winsta0\default).
		//Processes started by the logged - on user are associated with the Winsta0\default desktop.
		//The name of the desktop, or the name of both the desktop and window station for this process. 
		si.lpDesktop = "winsta0\\default";
		ZeroMemory(&pi, sizeof(pi));
		//opens the access token associated with a process.
		if (!::OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY
			| TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_ADJUST_SESSIONID
			| TOKEN_READ | TOKEN_WRITE, &hPToken))
		{
			dwRet = GetLastError();
			WarnLog(__FUNCTION__" OpenProcessToken error, %x", dwRet);
			break;
		}
		//retrieves the locally unique identifier (LUID) 
		// used on a specified system to locally represent the specified privilege name.
		if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid))
		{
			dwRet = GetLastError();
			WarnLog(__FUNCTION__" LookupPrivilegeValue error, %x", dwRet);
			break;
		}

		tp.PrivilegeCount = 1;
		tp.Privileges[0].Luid = luid;
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		//creates a new access token that duplicates an existing token
		if (!DuplicateTokenEx(hPToken, MAXIMUM_ALLOWED, NULL, SecurityIdentification, TokenPrimary, &hUserTokenDup))
		{
			dwRet = GetLastError();
			WarnLog(__FUNCTION__" DuplicateTokenEx error, %x", dwRet);
			break;
		}
		//Adjust Token privilege
		if (!SetTokenInformation(hUserTokenDup, TokenSessionId, (void*)&dwSessionId, sizeof(DWORD)))
		{
			dwRet = GetLastError();
			WarnLog(__FUNCTION__" SetTokenInformation error, %x", dwRet);
			break;
		}
		if (!AdjustTokenPrivileges(hUserTokenDup, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), (PTOKEN_PRIVILEGES)NULL, NULL))
		{
			dwRet = GetLastError();
			WarnLog(__FUNCTION__" AdjustTokenPrivileges error, %x", dwRet);
			break;
		}
		LPVOID pEnv = NULL;
		if (CreateEnvironmentBlock(&pEnv, hUserTokenDup, TRUE))
		{
			//the environment block pointed to by lpEnvironment uses Unicode characters.
			dwCreationFlags |= CREATE_UNICODE_ENVIRONMENT;
		}

		// Launch the process in the client's logon session.
		if (CreateProcessAsUserA(hUserTokenDup,    // client's access token
			NULL,        // file to execute
			lpCommand,        // command line
			NULL,            // pointer to process SECURITY_ATTRIBUTES
			NULL,            // pointer to thread SECURITY_ATTRIBUTES
			FALSE,            // handles are not inheritable
			dwCreationFlags,// creation flags
			pEnv,          // pointer to new environment block
			NULL,          // name of current directory
			&si,            // pointer to STARTUPINFO structure
			&pi            // receives information about new process
			))
		{
			InfoLog(__FUNCTION__" CreateProcessAsUser %s Success", lpCommand);
		}
		else
		{
			dwRet = GetLastError();
			WarnLog(" CreateProcessAsUser %s error, %x", lpCommand, dwRet);
			break;
		}
	} while (0);
	
	if (NULL != hUserToken)
	{
		CloseHandle(hUserToken);
	}
	else;

	if (NULL != hUserTokenDup)
	{
		CloseHandle(hUserTokenDup);
	}
	else;

	if (NULL != hPToken)
	{
		CloseHandle(hPToken);
	}

}