#include "WvDecoder.h"

using namespace std;

int can_seek(void *)
{
	return 1;
}

int64_t get_length(void *id)
{
	return ((CStream*)id)->Length();
}

int64_t get_pos(void *id)
{
	return ((CStream*)id)->Tell();
}

// Only used once in the wavpack source, and this should be sufficient
int push_back_byte(void *id, int c)
{
	if(((CStream*)id)->Seek(-1,SEEK_CUR))
		return EOF;
	else
		return c;
}

int32_t read_bytes(void *id, void *data, int32_t bcount)
{
	return ((CStream*)id)->Read(data,bcount);
}

int set_pos_abs(void *id, int64_t pos)
{
	return ((CStream*)id)->Seek(pos,SEEK_SET);
}

int set_pos_rel(void *id, int64_t delta, int mode)
{
	return ((CStream*)id)->Seek(delta,mode);
}

CWvDecoder::CWvDecoder()
{
	m_opened=0;
	m_error[0]=0;
}

CWvDecoder::~CWvDecoder()
{
	CloseFiles();
}

CWvDecoder::CWvDecoder(const wchar_t *filename)
{
	m_opened=0;
	m_error[0]=0;
	if(!m_stream.Open(filename,m_usemem))
		return;
	wstring str=filename;
	str+='c';
	CStream* wvc=0;
	if(m_stream2.Open(str.c_str(),m_usemem))
		wvc=&m_stream2;
	memset(&m_reader,0,sizeof(m_reader));
	m_reader.can_seek=can_seek;
	m_reader.get_length=get_length;
	m_reader.get_pos=get_pos;
	m_reader.push_back_byte=push_back_byte;
	m_reader.read_bytes=read_bytes;
	m_reader.set_pos_abs=set_pos_abs;
	m_reader.set_pos_rel=set_pos_rel;
	char error[80];
	m_wpc=WavpackOpenFileInputEx64(&m_reader,&m_stream,wvc,error,OPEN_WVC,0);
	if(m_wpc&&WavpackGetNumChannels(m_wpc)>0)
	{
		m_samples=WavpackGetNumSamples64(m_wpc);
		m_bps=WavpackGetBytesPerSample(m_wpc);
		m_channels=WavpackGetNumChannels(m_wpc);
		m_chunksize=WVSIZE/m_channels;
		int mode=WavpackGetMode(m_wpc);
		if((mode&MODE_MD5)&&(mode&MODE_LOSSLESS))
		{
			m_calcmd5=1;
			md5_init(&m_md5);
		}
		else
			m_calcmd5=0;
		m_count=0;
		m_opened=1;
	}
	else if(m_wpc)
		WavpackCloseFile(m_wpc);
}

void CWvDecoder::CloseFiles()
{
	if(m_opened)
		WavpackCloseFile(m_wpc);
	m_stream.Close();
	m_stream2.Close();
}

const wchar_t* CWvDecoder::GetSupportedTypes()
{
	return WVTYPES;
}

CDecoder* CWvDecoder::NewDecoder(const wchar_t* filename)
{
	CWvDecoder* d=new CWvDecoder(filename);
	if(d->m_opened)
		return d;
	else
	{
		delete d;
		return 0;
	}
}

__int64 CWvDecoder::GetSampleCount()
{
	return m_samples;
}

const wchar_t* CWvDecoder::GetLastError()
{
	return m_error;
}

long CWvDecoder::Read()
{
	long i=WavpackUnpackSamples(m_wpc,m_buffer,m_chunksize);
	if(i)
	{
		if(m_calcmd5)
			UpdateMD5(i*m_channels);
		m_count+=i;
	}
	else
	{
		int errors=WavpackGetNumErrors(m_wpc);
		if(errors)
		{
			swprintf_s(m_error,MAXERROR,L"%d BAD BLOC%s",errors,errors>1?L"KS":L"K");
			i=-1;
		}
		else
		{
			if(m_samples==-1||m_count==m_samples)	// -1 means the total is unknown
			{
				if(m_calcmd5)
				{
					unsigned char a[16];
					if(WavpackGetMD5Sum(m_wpc,a))
					{
						unsigned char b[16];
						md5_finish(&m_md5,b);
						if(memcmp(a,b,16))
						{
							wcscpy_s(m_error,MAXERROR,L"MD5 MISMATCH");
							return -1;
						}
					}
				}
			}
			else
			{
				swprintf_s(m_error,MAXERROR,L"%lld %s",m_count<m_samples?m_samples-m_count:m_count-m_samples,m_count<m_samples?L"MISSING SAMPLES":L"EXTRA SAMPLES");
				i=-1;
			}
		}
	}
	return i;
}

void CWvDecoder::UpdateMD5(long count)
{
	int32_t temp;
	int32_t* src=m_buffer;
	unsigned char* dst=(unsigned char*)m_buffer;
	long i=count;
	switch(m_bps)
	{
	case 1:
		while(i--)
			*dst++=(unsigned char)((*src++)+128);
		break;
	case 2:
	    while(i--)
		{
			*dst++ = (unsigned char)(temp = *src++);
			*dst++ = (unsigned char)(temp >> 8);
	    }
	    break;
	case 3:
	    while(i--)
		{
			*dst++ = (unsigned char)(temp = *src++);
			*dst++ = (unsigned char)(temp >> 8);
			*dst++ = (unsigned char)(temp >> 16);
	    }
	    break;
	case 4:
	    while(i--)
		{
			*dst++ = (unsigned char)(temp = *src++);
			*dst++ = (unsigned char)(temp >> 8);
			*dst++ = (unsigned char)(temp >> 16);
			*dst++ = (unsigned char)(temp >> 24);
	    }
	}
	md5_append(&m_md5,(unsigned char*)m_buffer,count*m_bps);
}
