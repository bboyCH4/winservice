#include <windows.h>
#include <stdio.h>

#define SVC_NAME "vTopSrv"
#define SVC_DISPLAYNAME "Halsign Tools for Virtual Machines Service"
#define SVC_DESC "Monitors and provides various metrics to XenStore"

static int ServiceInstall(const char* pServiceName);
//////////////////////////////////////////////////////////////////////////////////
//
// argv[1] must be one of the following flags:
//  '/p' -> argv[3] is the HardwareId and argv[4] is the full path to the INF
//          Call UpdateDriverForPlugAndPlayDevices to install drivers on a PnP
//          device.  Also check if any of the devices are currently phantoms and
//          if so mark them for reinstall so they will get their drivers installed
//          when they are reconnected to the machine.
//
//  '/i' -> argv[3] is the full path to the INF.
//          This preforms an INF install only by calling SetupCopyOEMInf.
//
//  '/r' -> argv[3] is the HardwareId and argv[4] is the full path to the INF
//          This option is used for root enumerated devices.  This api will first check
//          if there is a root enumerated device that matches the given HardwareId.  If
//          there is then it will simply update the drivers on that device by calling
//          UpdateDriverForPlugAndPlayDevices.  If it is not present yet it will create
//          the root enumerated device first and then call UpdateDriverForPlugAndPlayDevices
//          to install the drivers on it.
//  '/s' -> argv[3] is the full path to the service binary to install or update.
//          This option is used to install the xensvc service or to update its
//          configuration.
//  '/d' -> arg[3] is the full path to the INF file and argv[4] is the service section
//          to install from the INF file.
//          This option is used to install a driver service from a specified INF file.
// 
//////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
	if (argc < 2) 
	{
		printf("Invalid number of parameters passed in", "Error", MB_OK);
		return 1;
	}
	//原版中还有一个参数为窗口句柄给Messagebox使用
	//第一个参数是否为null，决定在任务栏中是否会多一个窗口
	if (!lstrcmpiA(argv[1], "/s")) 
	{
		return ServiceInstall(argv[2]);
	}
	else 
	{
		printf("Invalid device install type passed in");
		return 1;
	}
	return 0;
}


int ServiceInstall(const char* pServiceName)
{
	OSVERSIONINFOEXA				VersionInfo;
	ULONG							WindowsVersion = 0;
	SC_HANDLE						hMgr = NULL;
	SC_HANDLE						hSvc = NULL;
	SERVICE_DESCRIPTIONA			desc;
	SERVICE_FAILURE_ACTIONSA		actions;
	SC_ACTION						restartAction[3];
	SERVICE_FAILURE_ACTIONS_FLAG	flag;
	//VersionInfo.dwOSVersionInfoSize = sizeof(VersionInfo);
	//
	//if (GetVersionExA((OSVERSIONINFOA*)&VersionInfo)) 
	//{
	//	if (((VersionInfo.dwMajorVersion & ~0xff) == 0)
	//		&& ((VersionInfo.dwMinorVersion & ~0xff) == 0)) 
	//	{
	//		WindowsVersion = (VersionInfo.dwMajorVersion << 8) |
	//			VersionInfo.dwMinorVersion;
	//	}
	//}
	//AddEventSource(Service);
	//Establishes a connection to the service control manager on the local computer.
	hMgr = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	
	if (hMgr == NULL) 
	{
		printf("OpenSCManagerA failed %d\n",GetLastError());
		goto exit;
	}
	printf("%s\n", pServiceName);
	//服务名称servicename和显示名称displayname,都可以在系统服务查看
	//servicename：右键点击具体服务属性查看
	//displayname: 服务窗口显示的名称
	hSvc = OpenServiceA(hMgr, SVC_NAME, SERVICE_ALL_ACCESS);
	//未存在就创建，否则进行更新
	if (hSvc == NULL)
	{
		hSvc = CreateServiceA(hMgr,                      // SCManager database
			SVC_NAME,			        // name of service
			SVC_DISPLAYNAME,           // name to display
			SERVICE_ALL_ACCESS,        // desired access
			SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS,  // 不设置SERVICE_INTERACTIVE_PROCESS，创建
			//带GUI的会服务异常
			SERVICE_AUTO_START,		// start type
			SERVICE_ERROR_NORMAL,      // 记录到eventlog中
			pServiceName,                   // service's binary
			NULL,                      // no load ordering group
			NULL,                      // no tag identifier
			NULL,						//启动前必须运行的组件（这里去掉了）
			NULL,                      // LocalSystem account（运行在哪个account）
			NULL);                     // no password

		if (hSvc == NULL) 
		{
			printf("Failed to install the service %d\n", GetLastError());
			goto exit;
		}
		//para 3 传给serivice main的参数个数
		//para 4 传给serivice main的参数
		StartServiceA(hSvc, 0, NULL);
	}
	else
	{
		//
		// Service already exists, so just update its values.
		//
		if (!ChangeServiceConfigA(hSvc,
			SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS,
			SERVICE_AUTO_START,
			SERVICE_ERROR_NORMAL,
			pServiceName,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			SVC_DISPLAYNAME)) 
		{
			printf("Failed to update the servic %d\n", GetLastError());
			goto exit;
		}
	}//end if hSvc == NULL

	desc.lpDescription = SVC_DESC;
	if (!ChangeServiceConfig2A(hSvc, SERVICE_CONFIG_DESCRIPTION, &desc))
	{
		printf("Failed to SERVICE_CONFIG_DESCRIPTION %d\n", GetLastError());
	}
	//The action to be performed.
	restartAction[0].Type = SC_ACTION_RESTART;
	//The time to wait before performing the specified action, in milliseconds.
	restartAction[0].Delay = 4      // minutes
		* 60     // s
		* 1000;  // ms

	restartAction[1].Type = SC_ACTION_RESTART;
	restartAction[1].Delay = 8      // minutes
		* 60     // s
		* 1000;  // ms

	restartAction[2].Type = SC_ACTION_RESTART;
	restartAction[2].Delay = 12     // minutes
		* 60     // s
		* 1000;  // ms
	//The time after which to reset the failure count to zero if there are no failures,
	actions.dwResetPeriod = 3600;
	//The message to be broadcast to server users before rebooting in response to the SC_ACTION_REBOOT service controller action.
	actions.lpRebootMsg = NULL;
	actions.lpCommand = NULL;
	actions.cActions = sizeof(restartAction) / sizeof(restartAction[0]);
	actions.lpsaActions = restartAction;

	if (!ChangeServiceConfig2A(hSvc, SERVICE_CONFIG_FAILURE_ACTIONS, &actions))
	{
		printf("Failed to SERVICE_CONFIG_FAILURE_ACTIONS %d\n", GetLastError());
	}
	//Represents the action the service controller should take on each failure of a service.
	//A service is considered failed when it terminates without
	//reporting a status of SERVICE_STOPPED to the service controller.	
	flag.fFailureActionsOnNonCrashFailures = TRUE;

	ChangeServiceConfig2A(hSvc, SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, &flag);
exit:
	if (hSvc != NULL) {
		CloseServiceHandle(hSvc);
	}
	if (hMgr != NULL) 
	{
		CloseServiceHandle(hMgr);
	}
	return 0;
}