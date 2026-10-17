#include "FlacDecoder.h"

CFlacDecoder::CFlacDecoder()
{
	m_opened=0;
}

CFlacDecoder::~CFlacDecoder()
{
	if(m_opened)
	{
		finish();
		m_stream.Close();
	}
}

CFlacDecoder::CFlacDecoder(const wchar_t* filename)
{
	m_opened=0;
	m_error=0;
	if(m_stream.Open(filename,m_usemem))
	{
		set_md5_checking(true);
		if(init()==FLAC__STREAM_DECODER_INIT_STATUS_OK)
		{
			process_until_end_of_metadata();
			m_count=0;
			m_opened=1;
			return;
		}
		m_stream.Close();
	}
}

const wchar_t* CFlacDecoder::GetSupportedTypes()
{
	return FLACTYPES;
}

CDecoder* CFlacDecoder::NewDecoder(const wchar_t* filename)
{
	CFlacDecoder* d=new CFlacDecoder(filename);
	if(d->m_opened)
		return d;
	else
	{
		delete d;
		return 0;
	}
}

__int64 CFlacDecoder::GetSampleCount()
{
	return m_samples;
}

const wchar_t* CFlacDecoder::GetLastError()
{
	return m_str;
}

long CFlacDecoder::Read()
{
	m_framesize=0;
	if(!process_single()||m_error)
	{
		m_framesize=-1;
		if(!m_error)
		{
			switch(get_state())
			{
			case FLAC__STREAM_DECODER_END_OF_STREAM:
				Truncated();
				break;
			case FLAC__STREAM_DECODER_SEEK_ERROR:
				wcscpy_s(m_str,MAXERROR,L"SEEK_ERROR");
				break;
			case FLAC__STREAM_DECODER_ABORTED:
				wcscpy_s(m_str,MAXERROR,L"DECODER_ABORTED");
				break;
			case FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR:
				wcscpy_s(m_str,MAXERROR,L"MEMORY_ALLOCATION_ERROR");
				break;
			case FLAC__STREAM_DECODER_OGG_ERROR:
				wcscpy_s(m_str, MAXERROR, L"OGG_LAYER_ERROR");
				break;
			default:
				wcscpy_s(m_str,MAXERROR,L"DECODER_ERROR");
			}
		}
	}
	else
	{
		if(get_state()==FLAC__STREAM_DECODER_END_OF_STREAM)
		{
			if(!finish())
			{
				m_framesize=-1;
				wcscpy_s(m_str,MAXERROR,L"MD5_MISMATCH");
			}
			else
			{
				if(m_count!=m_samples)
				{
					m_framesize=-1;
					if(m_count<m_samples)
						Truncated();
					else
						wcscpy_s(m_str,MAXERROR,L"EXTRA SAMPLES");
				}
			}
		}
	}
	return m_framesize;
}

FLAC__StreamDecoderReadStatus CFlacDecoder::read_callback(FLAC__byte buf[],size_t * size)
{
	if(*size>0)
	{
		*size=m_stream.Read(buf,*size);
		if(*size>0)
			return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
		else
			return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
	}
	else
		return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
}

FLAC__StreamDecoderSeekStatus CFlacDecoder::seek_callback(FLAC__uint64 pos)
{
	if(m_stream.Seek(pos,SEEK_SET))
		return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
	else
		return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

FLAC__StreamDecoderTellStatus CFlacDecoder::tell_callback(FLAC__uint64 * pos)
{
	*pos=m_stream.Tell();
	return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

FLAC__StreamDecoderLengthStatus CFlacDecoder::length_callback(FLAC__uint64 * pos)
{
	*pos=m_stream.Length();
	return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

bool CFlacDecoder::eof_callback(void)
{
	if(m_stream.EndOfFile())
		return true;
	else
		return false;
}

FLAC__StreamDecoderWriteStatus CFlacDecoder::write_callback(const FLAC__Frame * frame,const FLAC__int32 *const buffer[])
{
	m_framesize=frame->header.blocksize;
	m_count+=m_framesize;
	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void CFlacDecoder::metadata_callback(const FLAC__StreamMetadata * metadata)
{
	if(metadata->type==FLAC__METADATA_TYPE_STREAMINFO)
	{
		m_rate=metadata->data.stream_info.sample_rate;
		m_samples=metadata->data.stream_info.total_samples;
	}
}

void CFlacDecoder::error_callback(FLAC__StreamDecoderErrorStatus error)
{
	m_error=1;
	unsigned int i=m_rate?(unsigned int)m_count/m_rate:0;
	switch(error)
	{
	case FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC:
		swprintf_s(m_str,MAXERROR,L"LOST_SYNC @ %dm %02ds",i/60,i%60);
		break;
	case FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER:
		swprintf_s(m_str,MAXERROR,L"BAD_HEADER @ %dm %02ds",i/60,i%60);
		break;
	case FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH:
		swprintf_s(m_str,MAXERROR,L"FRAME_CRC_MISMATCH @ %dm %02ds",i/60,i%60);
		break;
	case FLAC__STREAM_DECODER_ERROR_STATUS_UNPARSEABLE_STREAM:
		wcscpy_s(m_str,MAXERROR,L"UNPARSEABLE_STREAM");
	}
}

void CFlacDecoder::Truncated()
{
	unsigned int i=m_rate?(unsigned int)m_count/m_rate:0;
	swprintf_s(m_str,MAXERROR,L"TRUNCATED @ %dm %02ds",i/60,i%60);
}