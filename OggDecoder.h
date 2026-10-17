#pragma once

#include "decoders.h"
#include "stream.h"
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

const wchar_t OGGTYPES[]=L"ogg\0";

class COggDecoder :	public CDecoder
{
public:
	COggDecoder();
	COggDecoder(const wchar_t* filename);
	virtual ~COggDecoder();

	virtual const wchar_t* GetSupportedTypes();
	virtual CDecoder* NewDecoder(const wchar_t* filename);

	virtual __int64 GetSampleCount();
	virtual const wchar_t* GetLastError();
	virtual long Read();

	bool m_opened;

private:
	OggVorbis_File m_vf;
	ov_callbacks m_callback;
	CStream m_stream;
	__int64 m_samples;
	int m_bitstream;
	long m_rate;
	float** m_buf;
	wchar_t m_error[MAXERROR];
};
