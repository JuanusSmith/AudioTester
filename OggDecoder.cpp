#include "OggDecoder.h"
#include <string.h>

#define CHUNKSIZE	4096

size_t read_func(void *ptr, size_t size, size_t nmemb, void *datasource)
{
	return ((CStream*)datasource)->Read(ptr,(unsigned long)(size*nmemb));
}

int seek_func(void *datasource, ogg_int64_t offset, int whence)
{
	return ((CStream*)datasource)->Seek(offset,whence);
}

int close_func(void *datasource)
{
	((CStream*)datasource)->Close();
	return 1;
}

long tell_func(void *datasource)
{
	return (long)((CStream*)datasource)->Tell();
}

COggDecoder::COggDecoder()
{
	m_opened=0;
	m_error[0]=0;
}

COggDecoder::COggDecoder(const wchar_t* filename)
{
	m_opened=0;
	m_error[0]=0;
	m_callback.close_func=close_func;
	m_callback.read_func=read_func;
	m_callback.seek_func=seek_func;
	m_callback.tell_func=tell_func;
	if(m_stream.Open(filename,m_usemem))
	{
		if(!ov_open_callbacks(&m_stream,&m_vf,0,0,m_callback))
		{	
			vorbis_info* info=ov_info(&m_vf,-1);
			m_rate=info->rate;
			m_samples=ov_pcm_total(&m_vf,-1);
			m_opened=1;
			return;
		}
		m_stream.Close();
	}
}

COggDecoder::~COggDecoder()
{
	if(m_opened)
		ov_clear(&m_vf);
}

const wchar_t* COggDecoder::GetSupportedTypes()
{
	return OGGTYPES;
}

CDecoder* COggDecoder::NewDecoder(const wchar_t* filename)
{
	COggDecoder* d=new COggDecoder(filename);
	if(d->m_opened)
		return d;
	else
	{
		delete d;
		return 0;
	}
}

__int64 COggDecoder::GetSampleCount()
{
	return m_samples;
}

const wchar_t* COggDecoder::GetLastError()
{
	return m_error;
}

long COggDecoder::Read()
{
	long i=ov_read_float(&m_vf,&m_buf,CHUNKSIZE,&m_bitstream);
	switch(i)
	{
	case OV_HOLE:
		{
			float f=(float)ov_time_tell(&m_vf);
			swprintf_s(m_error,MAXERROR,L"OV_HOLE @ %dm %02ds",((int)f)/60,((int)f)%60);
		}
		return -1;
	case OV_EBADLINK:
		{
			float f=(float)ov_time_tell(&m_vf);
			swprintf_s(m_error,MAXERROR,L"OV_EBADLINK @ %dm %02ds",((int)f)/60,((int)f)%60);
		}
		return -1;
	case 0:
		if(!m_vf.os.e_o_s)
		{
			wcscpy_s(m_error,MAXERROR,L"TRUNCATED");
			return -1;
		}
		break;
	default:
		if(i<0)
		{
			swprintf_s(m_error,MAXERROR,L"DECODE ERROR (%ld)",i);
			return -1;
		}
	}
	return i;
}
