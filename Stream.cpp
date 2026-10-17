// A file i/o wrapper using available memory, to prevent disk thrashing between threads

#include "Stream.h"
#include <windows.h>
#include <io.h>

#define BLOCKSIZE	65536

CStream::CStream()
{
	m_file=0;
	m_buffer=0;
}

CStream::~CStream()
{
	Close();
}

bool CStream::Open(const wchar_t* filename, bool usemem)
{
	_wfopen_s(&m_file,filename,L"rb");
	if(m_file)
	{
		if(usemem)
		{
			int size=_filelength(_fileno(m_file));
			if(size)
			{
				m_bufsize=size;
				try {
					m_buffer=new unsigned char[size];
					int i;
					unsigned char* b=m_buffer;
					while(size>0)
					{
						i=(size<BLOCKSIZE)?size:BLOCKSIZE;
						if(fread(b,1,i,m_file)==i)
						{
							b+=i;
							size-=i;
						}
						else
						{
							delete [] m_buffer;
							m_buffer=0;
							break;
						}
					}
					if(m_buffer)
					{
						fclose(m_file);
						m_file=0;
						m_bufpos=0;
						m_eof=0;
					}
				}
				catch (...) {
					// Not enough memory.
				}
			}
		}
		return 1;
	}
	else
		return 0;
}

void CStream::Close()
{
	if(m_file)
	{
		fclose(m_file);
		m_file=0;
	}
	if(m_buffer)
	{
		delete [] m_buffer;
		m_buffer=0;
	}
}

unsigned long CStream::Read(void* buf, unsigned long size)
{
	if(m_buffer)
	{
		if(m_bufpos+size>m_bufsize)
		{
			size=m_bufsize-m_bufpos;
			m_eof=1;
		}
		if(size)
		{
			memcpy(buf,m_buffer+m_bufpos,size);
			m_bufpos+=size;
		}
		return size;
	}
	else
		return fread(buf,1,size,m_file);
}

int CStream::Seek(__int64 offset, long origin)
{
	if(m_buffer)
	{
		m_eof=0;
		switch(origin)
		{
		case SEEK_CUR:
			if(m_bufpos+offset<0||m_bufpos+offset>m_bufsize)
				return -1;
			else
			{
				m_bufpos=(unsigned long)(m_bufpos+offset);
				return 0;
			}
		case SEEK_SET:
			if(offset<0||offset>m_bufsize)
				return -1;
			else
			{
				m_bufpos=(unsigned long)offset;
				return 0;
			}
		case SEEK_END:
			if(offset>0||m_bufsize+offset<0)
				return -1;
			else
			{
				m_bufpos=(unsigned long)(m_bufsize+offset);
				return 0;
			}
		default:
			return -1;
		}
	}
	else
		return _fseeki64(m_file,offset,origin);
}

__int64 CStream::Tell()
{
	if(m_buffer)
		return m_bufpos;
	else
		return _ftelli64(m_file);
}

__int64 CStream::Length()
{
	if(m_buffer)
		return m_bufsize;
	else
		return _filelengthi64(_fileno(m_file));
}

int CStream::EndOfFile()
{
	if(m_buffer)
		return m_eof;
	else
		return feof(m_file);
}
