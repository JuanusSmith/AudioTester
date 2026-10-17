// A file i/o wrapper using available memory, to prevent disk thrashing between threads

#include "Stream.h"
#include <windows.h>
#include <io.h>
#include <new>

#define BLOCKSIZE	65536
#define MAXMEMFILE	(256*1024*1024)	// Larger files are read through a buffered FILE instead

// Whole-file loads are serialised so that threads read the disk one file at a time
static SRWLOCK g_loadlock=SRWLOCK_INIT;

CStream::CStream()
{
	m_file=0;
	m_buffer=0;
	m_eof=0;
	m_bufsize=0;
	m_bufpos=0;
}

CStream::~CStream()
{
	Close();
}

bool CStream::Open(const wchar_t* filename, bool usemem)
{
	Close();
	_wfopen_s(&m_file,filename,L"rb");
	if(m_file)
	{
		__int64 size=_filelengthi64(_fileno(m_file));
		if(usemem&&size>0&&size<=MAXMEMFILE)
		{
			m_buffer=new(std::nothrow) unsigned char[(size_t)size];
			if(m_buffer)
			{
				m_bufsize=size;
				unsigned char* b=m_buffer;
				AcquireSRWLockExclusive(&g_loadlock);
				while(size>0)
				{
					size_t i=(size<BLOCKSIZE)?(size_t)size:BLOCKSIZE;
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
				ReleaseSRWLockExclusive(&g_loadlock);
				if(m_buffer)
				{
					fclose(m_file);
					m_file=0;
					m_bufpos=0;
					m_eof=0;
					return 1;
				}
				// Read failed part way, so fall back to reading from the file
				m_bufsize=0;
				_fseeki64(m_file,0,SEEK_SET);
			}
		}
		setvbuf(m_file,0,_IOFBF,BLOCKSIZE);
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
	m_bufsize=0;
	m_bufpos=0;
	m_eof=0;
}

unsigned long CStream::Read(void* buf, unsigned long size)
{
	if(m_buffer)
	{
		if(m_bufpos+size>m_bufsize)
		{
			size=(unsigned long)(m_bufsize-m_bufpos);
			m_eof=1;
		}
		if(size)
		{
			memcpy(buf,m_buffer+m_bufpos,size);
			m_bufpos+=size;
		}
		return size;
	}
	else if(m_file)
		return (unsigned long)fread(buf,1,size,m_file);
	else
		return 0;
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
				m_bufpos+=offset;
				return 0;
			}
		case SEEK_SET:
			if(offset<0||offset>m_bufsize)
				return -1;
			else
			{
				m_bufpos=offset;
				return 0;
			}
		case SEEK_END:
			if(offset>0||m_bufsize+offset<0)
				return -1;
			else
			{
				m_bufpos=m_bufsize+offset;
				return 0;
			}
		default:
			return -1;
		}
	}
	else if(m_file)
		return _fseeki64(m_file,offset,origin);
	else
		return -1;
}

__int64 CStream::Tell()
{
	if(m_buffer)
		return m_bufpos;
	else if(m_file)
		return _ftelli64(m_file);
	else
		return -1;
}

__int64 CStream::Length()
{
	if(m_buffer)
		return m_bufsize;
	else if(m_file)
		return _filelengthi64(_fileno(m_file));
	else
		return 0;
}

int CStream::EndOfFile()
{
	if(m_buffer)
		return m_eof;
	else if(m_file)
		return feof(m_file);
	else
		return 1;
}
