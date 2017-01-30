#ifndef _WIN_EXCEPEION_H_
#define _WIN_EXCEPEION_H_
#include <windows.h>
#include <exception>
#include <stdio.h>
#include "types.h"

#define THROW(...) throw WinExceptionWithMessage( GetLastError() ,__FILE__, __LINE__,## __VA_ARGS__)
/*
*  自定义错误类型
*/
class WinExceptionWithMessage : public std::exception
{
private:
	static const uint16_t err_msg_len = 1024;			//错误消息长度
private:
	char		m_ErrMsg[err_msg_len];					//存储错误消息
	DWORD		m_ErrCode;								//错误码
	char		m_SourceFile[MAX_PATH];
	uint32_t	m_Line;
public:

	/*构造函数*/
	WinExceptionWithMessage(DWORD code, const char* soureFile, uint32_t Line, const char * f, ...)
	{
		m_ErrCode = code;
#ifdef _DEBUG
		strcpy_s(m_SourceFile, MAX_PATH, strrchr(soureFile, '\\') + 1);
#else
		strcpy_s(m_SourceFile, MAX_PATH, soureFile);
#endif
		m_Line = Line;
		memset(m_ErrMsg, 0, err_msg_len);
		va_list m;
		va_start(m, f);
		vsprintf_s(m_ErrMsg, err_msg_len, f, m);
		va_end(m);
		//	std::exception(m_ErrMsg);
	}
	/*默认构造函数*/
	WinExceptionWithMessage()
		: std::exception()
	{}
	/**/
	WinExceptionWithMessage(WinExceptionWithMessage const &other)
		: std::exception(other)
	{
		//不会调用构造函数初始化的
		memset(m_ErrMsg, 0, err_msg_len);
		m_ErrCode = other.m_ErrCode;
		memcpy_s(m_ErrMsg, err_msg_len, other.m_ErrMsg, strlen(other.m_ErrMsg));
		memset(m_SourceFile, 0, MAX_PATH);
		strcpy_s(m_SourceFile, MAX_PATH, other.m_SourceFile);
		m_Line = other.m_Line;
	}
	/*返回错误消息*/
	const char *what()const
	{
		return m_ErrMsg;
	}
	/*析构函数*/
	virtual ~WinExceptionWithMessage()
	{
	}

	/*tofix: 需要修正赋值操作符*/
	WinExceptionWithMessage & operator=(WinExceptionWithMessage &other)
	{
		std::exception::operator=(other);
		return *this;
	}
	/*取得errorCode*/
	inline DWORD GetErrorCode(){ return m_ErrCode; }
	inline uint32_t GetErrorLine(){ return m_Line; }
	inline const char* GetErrorFile(){ return m_SourceFile; }
}; // Ends WinExceptionWithMessage

#endif