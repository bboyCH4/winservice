#include <windows.h>
#include "LogModule.h"

static void ServiceUninstall();
static void ServiceMain(int argc, char** argv);
static void ServiceControlHandler(DWORD request);
static void ServiceControlManagerUpdate(DWORD dwExitCode, DWORD dwState);
static BOOL ServiceInit();

static SERVICE_STATUS				_ServiceStatus;
static SERVICE_STATUS_HANDLE		_hStatus;
static HANDLE						_hServiceExitEvent;

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
