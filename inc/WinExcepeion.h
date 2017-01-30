#ifndef _WIN_EXCEPEION_H_
#define _WIN_EXCEPEION_H_
#include <windows.h>
#include <exception>
#include <stdio.h>
#include "types.h"

#define THROW(...) throw WinExceptionWithMessage( GetLastError() ,__FILE__, __LINE__,## __VA_ARGS__)
/*
*  �Զ����������
*/
class WinExceptionWithMessage : public std::exception
{
private:
	static const uint16_t err_msg_len = 1024;			//������Ϣ����
private:
	char		m_ErrMsg[err_msg_len];					//�洢������Ϣ
	DWORD		m_ErrCode;								//������
	char		m_SourceFile[MAX_PATH];
	uint32_t	m_Line;
public:

	/*���캯��*/
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
	/*Ĭ�Ϲ��캯��*/
	WinExceptionWithMessage()
		: std::exception()
	{}
	/**/
	WinExceptionWithMessage(WinExceptionWithMessage const &other)
		: std::exception(other)
	{
		//������ù��캯����ʼ����
		memset(m_ErrMsg, 0, err_msg_len);
		m_ErrCode = other.m_ErrCode;
		memcpy_s(m_ErrMsg, err_msg_len, other.m_ErrMsg, strlen(other.m_ErrMsg));
		memset(m_SourceFile, 0, MAX_PATH);
		strcpy_s(m_SourceFile, MAX_PATH, other.m_SourceFile);
		m_Line = other.m_Line;
	}
	/*���ش�����Ϣ*/
	const char *what()const
	{
		return m_ErrMsg;
	}
	/*��������*/
	virtual ~WinExceptionWithMessage()
	{
	}

	/*tofix: ��Ҫ������ֵ������*/
	WinExceptionWithMessage & operator=(WinExceptionWithMessage &other)
	{
		std::exception::operator=(other);
		return *this;
	}
	/*ȡ��errorCode*/
	inline DWORD GetErrorCode(){ return m_ErrCode; }
	inline uint32_t GetErrorLine(){ return m_Line; }
	inline const char* GetErrorFile(){ return m_SourceFile; }
}; // Ends WinExceptionWithMessage

#endif