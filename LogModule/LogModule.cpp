/*
* Copyright (c) 2016
*
* tofix:改成类的方式;GetFileSize仍存在安全隐患;能不能改成宏，这样__FUNCTION__很方便
* 虽然日志函数很少有情况达到很大的情况;
* 动态库的全局变量是不会相互影响的，如果改成类的话，每个日志函数还要多带一个参数。没必要
*/



#include <Shlwapi.h>
#include <windows.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include "mutex.h"
#include "tinyxml.h"
#include "tinystr.h"
#include "LogModule.h"
#include <string>
/*---------------------------------- 类型声明 ---------------------------------*/

/*================================== 类型声明 =================================*/

/*---------------------------------- 本地全局变量 ---------------------------------*/
static int				_log_size = 0;							//单个日志大小(MB)
static int				_log_num = 0;							//日志文件个数
static LogFunc			_log = NULL;							//日志处理函数
static char				_log_dir[MAX_PATH] = {0};				//日志所在目录
static char				_log_path[MAX_PATH] = {0};				//日志具体路径
static int				_log_index = 0;							//当前记录日志的索引
static char				_config_path[MAX_PATH] = {0};			//日志配置文件路径
static char				_log_name[MAX_PATH] = {0};				//日志名称
static mutex_t			_mutex;
static int				_test = 0;								//测试不同模块之间调用此DLL是否会影响
static bool				_initialized = false;					//是否已经初始化，一个进程初始化一次
/*================================== 本地全局变量 =================================*/

/*---------------------------------- 宏定义 ---------------------------------*/
#define FORM_SIZE				4096				//格式长度
#define TIME_LEN				128					//时间长度
#define LOG_BUF_SIZE			2048				//最终日志信息的长度
/*================================== 宏定义 =================================*/




/*---------------------------------- 前向声明 ---------------------------------*/
static void defLogFunc(const char* prefix, const char * format, va_list m);
static LogFunc setLogFunc(LogFunc f);
static BOOL CreateDeepDirectory(const char* pPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);
/*================================== 前向声明 =================================*/


/*---------------------------------- 函数实现 ---------------------------------*/
/*	函数说明：取得配置文件的路径
 *	输入参数：szLogPath：保存路径的buf
 *			  len:		 buf的长度
 *	输出参数：
*/
static int getConfigPath(char *szLogPath,int len)
{
	if (NULL == szLogPath || len <= 0)
	{
		return 1;
	}

	char szmodulePath[MAX_PATH] = {0};

	if ( 0 == GetModuleFileNameA(NULL, szmodulePath, MAX_PATH))
	{
		printf("GetModuleFileNameA failed,err is %d", GetLastError());
		return 1;
	}
	PathRemoveFileSpecA(szmodulePath);
	sprintf_s(szLogPath, MAX_PATH,"%s\\configure.xml",szmodulePath);
	return 0;
}
/*	函数说明：初始化日志大小
 *	输入参数：
 *	输出参数：
*/
static int getLogSize(TiXmlElement* RootElement)
{
	int res = 1;

	for (TiXmlElement *ChildElement = RootElement->FirstChildElement();ChildElement ;
				ChildElement = ChildElement->NextSiblingElement())
	{
		if (strcmp("Log",ChildElement->Value()) == 0)
		{
			for (TiXmlElement *subChildElement = ChildElement->FirstChildElement();subChildElement ;
				subChildElement = subChildElement->NextSiblingElement())
			{
				if (strcmp("Size",subChildElement->Value()) == 0)
				{
					_log_size = atoi(subChildElement->GetText()) * 1024 * 1024;
					res = 0;
					goto end;
				}
			}
		}

	}
end:
	return res;
}

/*	函数说明：初始化日志大小
 *	输入参数：
 *	输出参数：
*/
static int getLogName(TiXmlElement* RootElement)
{
	int res = 1;

	for (TiXmlElement *ChildElement = RootElement->FirstChildElement();ChildElement ;
				ChildElement = ChildElement->NextSiblingElement())
	{
		if (strcmp("Log",ChildElement->Value()) == 0)
		{
			for (TiXmlElement *subChildElement = ChildElement->FirstChildElement();subChildElement ;
				subChildElement = subChildElement->NextSiblingElement())
			{
				if (strcmp("Name",subChildElement->Value()) == 0)
				{
					memcpy_s(_log_name, MAX_PATH, subChildElement->GetText(), strlen(subChildElement->GetText()));
					res = 0;
					goto end;
				}
			}
		}

	}
end:
	return res;
}

/*	函数说明：初始化日志数目
 *	输入参数：
 *	输出参数：
*/
static int getLogNum(TiXmlElement* RootElement)
{
	int res = 1;

	for (TiXmlElement *ChildElement = RootElement->FirstChildElement();ChildElement ;
				ChildElement = ChildElement->NextSiblingElement())
	{
		if (strcmp("Log",ChildElement->Value()) == 0)
		{
			for (TiXmlElement *subChildElement = ChildElement->FirstChildElement();subChildElement ;
				subChildElement = subChildElement->NextSiblingElement())
			{
				if (strcmp("Num",subChildElement->Value()) == 0)
				{
					_log_num = atoi(subChildElement->GetText());
					res = 0;
					goto end;
				}
			}
		}

	}
end:
	return res;
}

/*	函数说明：初始化日志存储路径
 *	输入参数：
 *	输出参数：
*/
static int getLogDir(TiXmlElement* RootElement)
{
	int res = 1;

	for (TiXmlElement *ChildElement = RootElement->FirstChildElement();ChildElement ;
				ChildElement = ChildElement->NextSiblingElement())
	{
		if (strcmp("Log",ChildElement->Value()) == 0)
		{
			for (TiXmlElement *subChildElement = ChildElement->FirstChildElement();subChildElement ;
				subChildElement = subChildElement->NextSiblingElement())
			{
				if (strcmp("Dir",subChildElement->Value()) == 0)
				{
					if (subChildElement->GetText())
					{
						memcpy_s(_log_dir, MAX_PATH, subChildElement->GetText(), strlen(subChildElement->GetText()));
						res = CreateDeepDirectory(_log_dir,NULL);
					}
					goto end;
				}
			}
		}

	}
end:
	return res;
}

/*	函数说明：从配置文件取得当前日志的索引
 *	输入参数：
 *	输出参数：
 *  特殊说明：当没有找到Index选项的时候，保持初始值0
*/
static int getLogCurrentIndex(TiXmlElement* RootElement)
{
	int res = 1;

	for (TiXmlElement *ChildElement = RootElement->FirstChildElement();ChildElement ;
				ChildElement = ChildElement->NextSiblingElement())
	{
		if (strcmp("Log",ChildElement->Value()) == 0)
		{
			for (TiXmlElement *subChildElement = ChildElement->FirstChildElement();subChildElement ;
				subChildElement = subChildElement->NextSiblingElement())
			{
				if (strcmp("Index",subChildElement->Value()) == 0)
				{
					_log_index = atoi(subChildElement->GetText());	
					res = 0;
					goto end;
				}
			}
		}

	}
end:
	return res;
}


/*	函数说明：初始化日志模块参数
 *	输入参数：
 *	输出参数：
*/
static int initLogConfigure(char *szLogPath,const char* name)
{
	int res = 0;
	bool bfind = false;
	TiXmlElement*	ChildElement = NULL;
	TiXmlDocument*	xml_Document = new TiXmlDocument(szLogPath);
	//打开配置文件失败，返回
	if ( false == xml_Document->LoadFile())
	{
	//	printf("%s", _xml_Document->ErrorDesc());
		res = 1;
		goto end;
	}
	TiXmlElement *RootElement = xml_Document->RootElement();

	for ( ChildElement = RootElement->FirstChildElement();ChildElement ;
				ChildElement = ChildElement->NextSiblingElement())
	{
		if (strcmp(name,ChildElement->Value()) == 0)
		{
			bfind = true;
			break;
		}
	}

	if (bfind)
	{
		getLogSize(ChildElement);
		getLogNum(ChildElement);
		getLogDir(ChildElement);
		getLogCurrentIndex(ChildElement);
		getLogName(ChildElement);
		res = 0;
	}
end:
	free(xml_Document);
	xml_Document = NULL;
	return res;
}
/*	函数说明：存储当前索引，没有Index节点就创建，否则就修改
 *	输入参数：
 *	输出参数：
*/
static int saveLogIndex(TiXmlElement* RootElement)
{
	bool bFind	= false;
	TiXmlElement *LogElement = NULL;
	char index[30] = {0};
	_itoa_s(_log_index,index, 30, 10);

	for (TiXmlElement *ChildElement = RootElement->FirstChildElement();ChildElement ;
				ChildElement = ChildElement->NextSiblingElement())
	{
		if (strcmp("Log",ChildElement->Value()) == 0)
		{
			LogElement = ChildElement;

			for (TiXmlElement *subChildElement = ChildElement->FirstChildElement();subChildElement ;
				subChildElement = subChildElement->NextSiblingElement())
			{
				if (strcmp("Index",subChildElement->Value()) == 0)
				{//setValue修改的是节点的内容
					subChildElement->Clear();
					subChildElement->LinkEndChild(new TiXmlText(index));
					bFind = true;
					return 0;
				}
			}
		}
	}
	
	if (false == bFind && LogElement != NULL)
	{
		TiXmlElement * subIndexElement = new TiXmlElement("Index");
		subIndexElement->LinkEndChild(new TiXmlText(index));
		LogElement->LinkEndChild(subIndexElement);
	}
	return 0;
}

/*	函数说明：从配置文件中读取相关参数来初始化日志模块
 *	输入参数：
 *	输出参数：
*/
int _LOG_CALLCONV LoadLogModule(const char* name, LogFunc f)
{
	//只初始化一次就好
	if (_initialized)
		return 2;
	_initialized = true;
	//初始化互斥锁
	MUTEX_INIT(_mutex);

	if (getConfigPath(_config_path, MAX_PATH))
		return 1;

	if (initLogConfigure(_config_path,name))
		return 1;
	//设置日志函数
	setLogFunc(f);
	return 0;
}
/*	函数说明：卸载日志模块
 *	输入参数：
 *	输出参数：
 *  存在多线程保存结果的情况，加上互斥锁
*/
int	_LOG_CALLCONV UnloadLogModule()
{
	int res = 0;
	MUTEX_LOCK(_mutex);
	TiXmlDocument* xml_Document = new TiXmlDocument(_config_path);
	//打开配置文件失败，返回
	if ( false == xml_Document->LoadFile())
	{
	//	printf("%s", _xml_Document->ErrorDesc());
		res = 1;
		goto end;
	}
	TiXmlElement *RootElement = xml_Document->RootElement();
	//写入xml当前的index
	saveLogIndex(RootElement);
	xml_Document->SaveFile(_config_path);  
	res = 0;
end:
	free(xml_Document);
	xml_Document = NULL;
	MUTEX_UNLOCK(_mutex);
	return res;
}
/*	函数说明：调试信息函数
 *	输入参数：
 *	输出参数：
*/
void _LOG_CALLCONV DebugLog(const char * f, ...)
{
  if (_log)
  {
    va_list m;
    va_start(m, f);
    _log("Debug", f, m);
	va_end(m);
  }
}
/*	函数说明：普通信息函数
 *	输入参数：
 *	输出参数：
*/
void _LOG_CALLCONV InfoLog(const char * f, ...)
{
  if (_log)
  {
    va_list m;
    va_start(m, f);
    _log("Info", f, m);
	va_end(m);
  }
}
/*	函数说明：警告信息函数
 *	输入参数：
 *	输出参数：
*/
void _LOG_CALLCONV WarnLog(const char * f, ...)
{
  if (_log)
  {
    va_list m;
    va_start(m, f);
    _log("Warning", f, m);
	va_end(m);
  }
}
/*	函数说明：错误信息函数
 *	输入参数：
 *	输出参数：
*/
void _LOG_CALLCONV ErrorLog(const char * f, ...)
{
  if (_log)
  {
    va_list m;
    va_start(m, f);
    _log("Error", f, m);
	va_end(m);
  }
}

/*	函数说明：设置该模块模块的处理函数
 *	输入参数：
 *	输出参数：
 *  返回值	：返回先前的函数
*/
static LogFunc setLogFunc(LogFunc f)
{
	LogFunc r;

	if ( NULL == f)
	{
		_log = defLogFunc;
		return defLogFunc;
	}
	else
	{
		return r = _log, _log = f, r;
	}
	
}
/*	函数说明：优化的日志申请函数
 *	输入参数：
 *	输出参数：
*/
static void *myMalloc(size_t size)
{
	void *temp = NULL;
	temp = malloc(size);

	if (temp)
	{
		memset(temp, 0, size);
	}
	return temp;
}
/*	函数说明：取得当前系统时间
 *	输入参数：
 *	输出参数：
*/
static char *Get_system_time()
{
	char *time_string = NULL;
#ifdef WIN32
	SYSTEMTIME systime;
	GetLocalTime(&systime);
	time_string = (char *)myMalloc(TIME_LEN);	// Should be WAY more than enough!
	
	if (time_string == NULL) 
	{
		return NULL;
	}
	sprintf_s(time_string, TIME_LEN, "%d-%.2d-%.2d  %d:%.2d:%.2d.%.3d",
		systime.wYear, systime.wMonth, systime.wDay, systime.wHour,
		systime.wMinute, systime.wSecond, systime.wMilliseconds);

	return time_string;
#else
	
#endif
}

/*	函数说明：取得文件的大小
 *	输入参数：
 *	输出参数：
*/
static int getFileSize(const char* logPath)
{
	int size = 0;
	struct _stat buf;
	int result = 0;

	result = _stat( logPath, &buf );
	return result != 0 ? -1 : buf.st_size;
}

/*	函数说明：设置日志的路径
 *	输入参数：
 *	输出参数：
 *  返回值  ： 0成功 1失败
*/
static int setLogPath()
{
	if (_log_dir == NULL || _log_num == 0)
	{
		return 1;
	}
	char tempLogPath[MAX_PATH] = {0};
	int size = 0;								//temp测试
	//先生成临时的日志文件路径
	sprintf_s(tempLogPath, MAX_PATH, "%s\\%s_%d.txt",_log_dir,_log_name,_log_index + 1);
	size = getFileSize(tempLogPath);
	//如果文件不存在或者存在的话大小小于限制，那么写入该日志文件
	if (!PathFileExistsA(tempLogPath) || size < _log_size)
	{
		memcpy_s(_log_path, MAX_PATH, tempLogPath, MAX_PATH);
		return 0;
	}
	else//文件已经存在，切大小超过了限制
	{
		_log_index ++;
		
		if (_log_index < _log_num)
		{   //说明没超过数目
			sprintf_s(_log_path, MAX_PATH, "%s\\%s_%d.txt",_log_dir,_log_name,_log_index + 1);
			//说明要重新写了
			if (PathFileExistsA(_log_path))
			{
				DeleteFileA(_log_path);
				return 0;
			}
		}
		else
		{
			_log_index = 0;
			sprintf_s(_log_path, MAX_PATH, "%s\\%s_%d.txt",_log_dir,_log_name,_log_index + 1);
			DeleteFileA(_log_path);
			return 0;
		}
		return 0;
	}
}
/*	函数说明：默认的日志函数
 *	输入参数：
 *	输出参数：
*/
static void defLogFunc(const char* prefix, const char * format, va_list m)
{
	MUTEX_LOCK(_mutex);
	if (setLogPath())
	{
		MUTEX_UNLOCK(_mutex);
		return ;
	}

	if (_log_path == NULL)
	{
		MUTEX_UNLOCK(_mutex);
		return ;
	}
	char *szTmp = NULL,*szTime = NULL,*szFinal = NULL;
	FILE *fFile = NULL;
	errno_t err = 0;

	szTmp	= (char *)myMalloc(FORM_SIZE);
	szTime	= (char *)Get_system_time();
	szFinal = (char *)myMalloc(LOG_BUF_SIZE);
	//有一个申请失败就退出
	if (!szTmp || !szTime || !szFinal)
	{
		MUTEX_UNLOCK(_mutex);
		return ;
	}
	vsprintf_s(szTmp, FORM_SIZE, format, m);					
 
	sprintf_s(szFinal, LOG_BUF_SIZE, "|%s|%s|: %s\r",prefix, szTime, szTmp);
#ifdef _SELF_TEST
	printf("%s\n", szFinal);
#else
	if ((err = fopen_s(&fFile, _log_path, "a")) != NULL)
	{
		MUTEX_UNLOCK(_mutex);
		return;
	}

	fprintf(fFile,"%s\n",szFinal);
	fclose(fFile);
#endif
	if (szTmp)
	{
		free(szTmp);
		szTmp = NULL;
	}

	if (szTime)
	{
		free(szTime);
		szTime = NULL;
	}
	if (szFinal){
		free(szFinal);
		szFinal = NULL;
	}
	MUTEX_UNLOCK(_mutex);
}

void _LOG_CALLCONV GlobalTest()
{
	_test++;
	printf("%d", _test);
}


static BOOL CreateDeepDirectory(const char* pPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
	if (PathFileExistsA(pPathName))     
	{
		return TRUE;
	}
	char			szDirectory[MAX_PATH] = { 0 };
	char*			pNextToken = NULL;
	char*			pDirectory = NULL;
	const char*		pDelim = "\\";
	std::string		strDirectory = "";

	memcpy_s(szDirectory, MAX_PATH, pPathName, strlen(pPathName));
	pDirectory = strtok_s(szDirectory, pDelim, &pNextToken);

	while (pDirectory)
	{
		strDirectory += pDirectory;
		strDirectory += "\\";

		if (!PathFileExistsA(strDirectory.c_str()))
		{     //创建失败时还应删除已创建的上层目录，此次略
			if (!CreateDirectoryA(strDirectory.c_str(), lpSecurityAttributes))
			{
				WarnLog(__FUNCTION__" :CreateDirectory for %s Failed with ErrorCode is %d", strDirectory.c_str(), GetLastError());
				return FALSE;
			}
		}
		pDirectory = strtok_s(NULL, pDelim, &pNextToken);
	}
	return TRUE;
}
/*================================== 函数实现 =================================*/