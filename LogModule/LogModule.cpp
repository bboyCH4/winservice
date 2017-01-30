/*
* Copyright (c) 2016
*
* tofix:�ĳ���ķ�ʽ;GetFileSize�Դ��ڰ�ȫ����;�ܲ��ܸĳɺ꣬����__FUNCTION__�ܷ���
* ��Ȼ��־��������������ﵽ�ܴ�����;
* ��̬���ȫ�ֱ����ǲ����໥Ӱ��ģ�����ĳ���Ļ���ÿ����־������Ҫ���һ��������û��Ҫ
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
/*---------------------------------- �������� ---------------------------------*/

/*================================== �������� =================================*/

/*---------------------------------- ����ȫ�ֱ��� ---------------------------------*/
static int				_log_size = 0;							//������־��С(MB)
static int				_log_num = 0;							//��־�ļ�����
static LogFunc			_log = NULL;							//��־������
static char				_log_dir[MAX_PATH] = {0};				//��־����Ŀ¼
static char				_log_path[MAX_PATH] = {0};				//��־����·��
static int				_log_index = 0;							//��ǰ��¼��־������
static char				_config_path[MAX_PATH] = {0};			//��־�����ļ�·��
static char				_log_name[MAX_PATH] = {0};				//��־����
static mutex_t			_mutex;
static int				_test = 0;								//���Բ�ͬģ��֮����ô�DLL�Ƿ��Ӱ��
static bool				_initialized = false;					//�Ƿ��Ѿ���ʼ����һ�����̳�ʼ��һ��
/*================================== ����ȫ�ֱ��� =================================*/

/*---------------------------------- �궨�� ---------------------------------*/
#define FORM_SIZE				4096				//��ʽ����
#define TIME_LEN				128					//ʱ�䳤��
#define LOG_BUF_SIZE			2048				//������־��Ϣ�ĳ���
/*================================== �궨�� =================================*/




/*---------------------------------- ǰ������ ---------------------------------*/
static void defLogFunc(const char* prefix, const char * format, va_list m);
static LogFunc setLogFunc(LogFunc f);
static BOOL CreateDeepDirectory(const char* pPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);
/*================================== ǰ������ =================================*/


/*---------------------------------- ����ʵ�� ---------------------------------*/
/*	����˵����ȡ�������ļ���·��
 *	���������szLogPath������·����buf
 *			  len:		 buf�ĳ���
 *	���������
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
/*	����˵������ʼ����־��С
 *	���������
 *	���������
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

/*	����˵������ʼ����־��С
 *	���������
 *	���������
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

/*	����˵������ʼ����־��Ŀ
 *	���������
 *	���������
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

/*	����˵������ʼ����־�洢·��
 *	���������
 *	���������
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

/*	����˵�����������ļ�ȡ�õ�ǰ��־������
 *	���������
 *	���������
 *  ����˵������û���ҵ�Indexѡ���ʱ�򣬱��ֳ�ʼֵ0
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


/*	����˵������ʼ����־ģ�����
 *	���������
 *	���������
*/
static int initLogConfigure(char *szLogPath,const char* name)
{
	int res = 0;
	bool bfind = false;
	TiXmlElement*	ChildElement = NULL;
	TiXmlDocument*	xml_Document = new TiXmlDocument(szLogPath);
	//�������ļ�ʧ�ܣ�����
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
/*	����˵�����洢��ǰ������û��Index�ڵ�ʹ�����������޸�
 *	���������
 *	���������
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
				{//setValue�޸ĵ��ǽڵ������
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

/*	����˵�����������ļ��ж�ȡ��ز�������ʼ����־ģ��
 *	���������
 *	���������
*/
int _LOG_CALLCONV LoadLogModule(const char* name, LogFunc f)
{
	//ֻ��ʼ��һ�ξͺ�
	if (_initialized)
		return 2;
	_initialized = true;
	//��ʼ��������
	MUTEX_INIT(_mutex);

	if (getConfigPath(_config_path, MAX_PATH))
		return 1;

	if (initLogConfigure(_config_path,name))
		return 1;
	//������־����
	setLogFunc(f);
	return 0;
}
/*	����˵����ж����־ģ��
 *	���������
 *	���������
 *  ���ڶ��̱߳���������������ϻ�����
*/
int	_LOG_CALLCONV UnloadLogModule()
{
	int res = 0;
	MUTEX_LOCK(_mutex);
	TiXmlDocument* xml_Document = new TiXmlDocument(_config_path);
	//�������ļ�ʧ�ܣ�����
	if ( false == xml_Document->LoadFile())
	{
	//	printf("%s", _xml_Document->ErrorDesc());
		res = 1;
		goto end;
	}
	TiXmlElement *RootElement = xml_Document->RootElement();
	//д��xml��ǰ��index
	saveLogIndex(RootElement);
	xml_Document->SaveFile(_config_path);  
	res = 0;
end:
	free(xml_Document);
	xml_Document = NULL;
	MUTEX_UNLOCK(_mutex);
	return res;
}
/*	����˵����������Ϣ����
 *	���������
 *	���������
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
/*	����˵������ͨ��Ϣ����
 *	���������
 *	���������
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
/*	����˵����������Ϣ����
 *	���������
 *	���������
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
/*	����˵����������Ϣ����
 *	���������
 *	���������
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

/*	����˵�������ø�ģ��ģ��Ĵ�����
 *	���������
 *	���������
 *  ����ֵ	��������ǰ�ĺ���
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
/*	����˵�����Ż�����־���뺯��
 *	���������
 *	���������
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
/*	����˵����ȡ�õ�ǰϵͳʱ��
 *	���������
 *	���������
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

/*	����˵����ȡ���ļ��Ĵ�С
 *	���������
 *	���������
*/
static int getFileSize(const char* logPath)
{
	int size = 0;
	struct _stat buf;
	int result = 0;

	result = _stat( logPath, &buf );
	return result != 0 ? -1 : buf.st_size;
}

/*	����˵����������־��·��
 *	���������
 *	���������
 *  ����ֵ  �� 0�ɹ� 1ʧ��
*/
static int setLogPath()
{
	if (_log_dir == NULL || _log_num == 0)
	{
		return 1;
	}
	char tempLogPath[MAX_PATH] = {0};
	int size = 0;								//temp����
	//��������ʱ����־�ļ�·��
	sprintf_s(tempLogPath, MAX_PATH, "%s\\%s_%d.txt",_log_dir,_log_name,_log_index + 1);
	size = getFileSize(tempLogPath);
	//����ļ������ڻ��ߴ��ڵĻ���СС�����ƣ���ôд�����־�ļ�
	if (!PathFileExistsA(tempLogPath) || size < _log_size)
	{
		memcpy_s(_log_path, MAX_PATH, tempLogPath, MAX_PATH);
		return 0;
	}
	else//�ļ��Ѿ����ڣ��д�С����������
	{
		_log_index ++;
		
		if (_log_index < _log_num)
		{   //˵��û������Ŀ
			sprintf_s(_log_path, MAX_PATH, "%s\\%s_%d.txt",_log_dir,_log_name,_log_index + 1);
			//˵��Ҫ����д��
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
/*	����˵����Ĭ�ϵ���־����
 *	���������
 *	���������
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
	//��һ������ʧ�ܾ��˳�
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
		{     //����ʧ��ʱ��Ӧɾ���Ѵ������ϲ�Ŀ¼���˴���
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
/*================================== ����ʵ�� =================================*/