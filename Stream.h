#pragma once

#include <stdio.h>

class CStream
{
public:
	CStream();
	virtual ~CStream();

	bool Open(const wchar_t* filename, bool usemem=1);
	void Close();
	unsigned long Read(void* buf, unsigned long size);
	int Seek(__int64 offset, long origin);
	__int64 Tell();
	__int64 Length();
	int EndOfFile();

private:
	FILE* m_file;
	bool m_eof;
	unsigned char* m_buffer;
	unsigned long m_bufsize;
	unsigned long m_bufpos;
};
