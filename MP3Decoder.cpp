#include "MP3Decoder.h"

CMP3Decoder::CMP3Decoder()
{
	m_opened=0;
}

CMP3Decoder::CMP3Decoder(const wchar_t* filename)
{
	m_opened=0;
	m_error[0]=0;
	if(m_stream.Open(filename,m_usemem))
	{
		m_opened=1;
		m_filesize=m_stream.Length();
		m_headerlength=GetHeaderLength();
		m_footerlength=GetFooterLength();
		m_filesize-=m_footerlength;
		m_stream.Seek(m_headerlength,SEEK_SET);
		m_offset=m_headerlength;
		m_samplepos=0;
		m_lastheader=0;
		m_resync=MAXRESYNC;
	}
}

CMP3Decoder::~CMP3Decoder()
{
	if(m_opened)
		m_stream.Close();
}

const wchar_t* CMP3Decoder::GetSupportedTypes()
{
	return MP3TYPES;
}

CDecoder* CMP3Decoder::NewDecoder(const wchar_t* filename)
{
	CMP3Decoder* d=new CMP3Decoder(filename);
	if(d->m_opened)
		return d;
	else
	{
		delete d;
		return 0;
	}
}

__int64 CMP3Decoder::GetSampleCount()
{
	return m_filesize;
}

const wchar_t* CMP3Decoder::GetLastError()
{
	return m_error;
}

long CMP3Decoder::Read()
{
	m_framelength=4;
	while(--m_framelength>=0)
	{
		if(!m_stream.Read(((unsigned char*)&m_header)+m_framelength,1))
		{
			wcscpy_s(m_error,MAXERROR,L"LOST SYNC @ END OF FILE");
			return -1;
		}
	}
	m_framelength=GetFrameLength();
	if(m_framelength)
	{
		if((m_header&0xfffe0c00)==(m_lastheader&0xfffe0c00)||(!m_lastheader))
		{
			if(!m_lastheader)
			{
				m_lastheader=m_header;
				m_rate=m_samplerate;
			}
			m_offset+=m_framelength;
			if(m_offset<m_filesize)
			{
				if(CheckCRC())
				{
					m_stream.Seek(m_offset,SEEK_SET);
					return m_framelength;
				}
				else
					return -1;
			}
			else
			{
				if(m_offset==m_filesize)
					return (CheckCRC()?0:-1);
				else
					wcscpy_s(m_error,MAXERROR,L"TRUNCATED");
			}
		}
	}
	if(!wcslen(m_error))
	{
		if(m_samplepos)
		{
			if(Resync())
			{
				float f=(float)m_samplepos;
				f/=m_rate;
				swprintf_s(m_error,MAXERROR,L"LOST SYNC @ %dm %02ds",((int)f)/60,((int)f)%60);
			}
			else
				wcscpy_s(m_error,MAXERROR,L"LOST SYNC @ END OF FILE");
		}
		else
		{
			if(!Resync())
				wcscpy_s(m_error,MAXERROR,L"UNRECOGNISED FORMAT");
			else
			{
				if(m_headerlength)
					wcscpy_s(m_error,MAXERROR,L"BAD ID3v2 TAG");
				else
					wcscpy_s(m_error,MAXERROR,L"BAD STARTING SYNC");
			}
		}
	}
	return -1;
}

int CMP3Decoder::GetHeaderLength()		// Check for ID3v2 tag
{
	int i=0;
	char buf[10];
	if(m_stream.Read(buf,10)==10)
	{
		if((buf[0]=='I')&&(buf[1]=='D')&&(buf[2]=='3'))
		{
			if((buf[3]<0xff)&&(buf[4]<0xff)&&(buf[6]<0x80)&&(buf[7]<0x80)&&(buf[8]<0x80)&&(buf[9]<0x80))
			{
				i=buf[6];i<<=7;i+=buf[7];i<<=7;i+=buf[8];i<<=7;i+=buf[9];i+=((buf[3]==4)&&(buf[5]&0x10))?20:10;
			}
			if(!i)
				wcscpy_s(m_error,MAXERROR,L"BAD ID3v2 TAG");
		}
	}
	return i;
}

int CMP3Decoder::GetFooterLength()
{
	m_offset=0;
	if(!m_stream.Seek(-128,SEEK_END))							// Check for ID3v1 tag
	{
		char id3[4]="";
		m_stream.Read(id3,3);
		if(!strcmp(id3,"TAG"))
		{
			m_offset-=128;
			if(!m_stream.Seek(m_offset-9,SEEK_END))				// Check for LYRICS tag
				m_offset-=GetLyricsLength();
		}
	}
	if(!m_stream.Seek(m_offset-APE_TAG_FOOTER_BYTES,SEEK_END))	// Check for APE tag
	{
		APE_TAG_FOOTER footer;
		if((m_stream.Read(&footer,APE_TAG_FOOTER_BYTES)==APE_TAG_FOOTER_BYTES)&&(!strncmp(footer.ID,APE_TAG_FOOTER_ID,8)))
		{
			if(footer.Size<APE_TAG_FOOTER_BYTES||footer.Size>m_filesize)
				wcscpy_s(m_error,MAXERROR,L"BAD APE TAG");
			else
			{
				m_offset-=footer.Size;
				if(footer.Flags&APE_TAG_FLAG_CONTAINS_HEADER)
					m_offset-=APE_TAG_FOOTER_BYTES;
				if(!m_stream.Seek(m_offset-9,SEEK_END))			// Check for LYRICS tag
					m_offset-=GetLyricsLength();
			}
		}
	}
	return 0-(int)m_offset;
}

int CMP3Decoder::GetLyricsLength()
{
	char buf[12];
	buf[9]=0;
	if(m_stream.Read(buf,9)==9)
	{
		if(!strcmp(buf,"LYRICSEND"))
		{
			if(!m_stream.Seek(-5100,SEEK_CUR))
			{
				char buf2[5100];
				if(m_stream.Read(buf2,5100)==5100)
				{
					buf2[5099]=0;
					char* c=strstr(buf2,"LYRICSBEGIN");
					if(c)
						return 5100-(c-buf2);
					else
						wcscpy_s(m_error,MAXERROR,L"BAD LYRICS3v1 TAG");
				}
			}
		}
		else
		{
			if((!strcmp(buf,"LYRICS200"))&&(!m_stream.Seek(-15,SEEK_CUR))&&(m_stream.Read(buf,6)==6))
			{
				buf[6]=0;
				int length=atoi(buf);
				buf[11]=0;
				if((length)&&(!m_stream.Seek(0-6-length,SEEK_CUR))&&(m_stream.Read(buf,11)==11)&&(!strcmp(buf,"LYRICSBEGIN")))
					return length+15;
				else
					wcscpy_s(m_error,MAXERROR,L"BAD LYRICS3v2 TAG");
			}
		}
	}
	return 0;
}

int CMP3Decoder::GetFrameLength()
{
	if(m_header>0xffe00000)
	{
		m_version=(m_header>>19)&0x03;
		m_layer=(m_header>>17)&0x03;
		m_bitrate=(m_header>>12)&0x0f;
		m_samplerate=(m_header>>10)&0x03;
		m_padding=(m_header>>9)&0x01;
		m_mono=((m_header>>6)&0x03)==0x03;
		m_crc=!((m_header>>16)&0x01);
		m_column=5;
		switch(m_version)
		{
		case MPEG1:
			switch(m_layer)
			{
			case LAYER1:
				m_column=0;
				break;
			case LAYER2:
				m_column=1;
				break;
			case LAYER3:
				m_column=2;
			}
			break;
		case MPEG2:
		case MPEG2_5:
			switch(m_layer)
			{
			case LAYER1:
				m_column=3;
				break;
			case LAYER2:
			case LAYER3:
				m_column=4;
			}
		}
		if(m_column<5)
			m_bitrate=1000*MP3_BITRATES[m_bitrate][m_column];
		else
			return 0;
		switch(m_version)
		{
		case MPEG1:
			m_column=0;
			break;
		case MPEG2:
			m_column=1;
			break;
		case MPEG2_5:
			m_column=2;
			break;
		default:
			return 0;
		}
		m_samplerate=MP3_SAMPRATES[m_samplerate][m_column];
		if(!m_samplerate)
			return 0;
		switch(m_layer)
		{
		case LAYER1:
			m_samplepos+=384;
			return (12*m_bitrate/m_samplerate+m_padding)*4;
		case LAYER2:
			m_samplepos+=1152;
			return 144*m_bitrate/m_samplerate+m_padding;
		case LAYER3:
			if(m_version==MPEG1)
			{
				m_samplepos+=1152;
				return 144*m_bitrate/m_samplerate+m_padding;
			}
			else
			{
				m_samplepos+=576;
				return 72*m_bitrate/m_samplerate+m_padding;
			}
		}
	}
	return 0;
}

bool CMP3Decoder::Resync()
{
	m_header=0;
	m_framelength=0;
	while(!m_framelength&&--m_resync)
	{
		m_header<<=8;
		if(m_stream.Read((unsigned char*)&m_header,1))
			m_framelength=GetFrameLength();
		else
			return 0;
	}
	if(m_framelength)
	{
		if(m_lastheader)
		{
			if((m_header&0xfffe0c00)==(m_lastheader&0xfffe0c00))
				return 1;
			else
				return Resync();
		}
		else
		{
			m_lastheader=m_header;
			return Resync();
		}
	}
	else
		return 0;
}

bool CMP3Decoder::CheckCRC()
{
	if(m_crc&&m_layer==LAYER3)
	{
		m_crcbytes=MP3_CRCBYTES[m_mono?1:0][m_version==MPEG1?0:1];
		if((!m_stream.Seek(-2,SEEK_CUR))&&(m_stream.Read(m_crcbuf,2)==2)&&(m_stream.Read((unsigned char*)&m_crc+1,1))&&(m_stream.Read((unsigned char*)&m_crc,1))&&(m_stream.Read(m_crcbuf+2,m_crcbytes)==m_crcbytes)&&(CRC16(0xffff,m_crcbuf,m_crcbytes+2)!=m_crc))
		{
			float f=(float)m_samplepos;
			f/=m_rate;
			swprintf(m_error,MAXERROR,L"CRC ERROR @ %dm %02ds",((int)f)/60,((int)f)%60);
			return 0;
		}
	}
	return 1;
}

unsigned short CMP3Decoder::CRC16(int crc, unsigned char* buf, int length)	// from mpck
{
	int i,j,k;
	for(i=0;i<length;i++)
	{
		k=buf[i]<<8;
		for(j=0;j<8;j++)
		{
			k<<=1;
			crc<<=1;
			if(((crc^k)&0x10000))
				crc^=0x8005;
		}
	}
    return crc&0xffff;
}
