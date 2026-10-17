#pragma once

#include "decoders.h"
#include "stream.h"
#include "md5.h"
#include <wavpack/wavpack.h>

#define WVSIZE	4096
const wchar_t WVTYPES[]=L"wv\0";

class CWvDecoder : public CDecoder
{
public:
	CWvDecoder();
	CWvDecoder(const wchar_t* filename);
	virtual ~CWvDecoder();

	virtual const wchar_t* GetSupportedTypes();
	virtual CDecoder* NewDecoder(const wchar_t* filename);

	virtual __int64 GetSampleCount();
	virtual const wchar_t* GetLastError();
	virtual long Read();

	bool m_opened;

private:
	__int64 m_samples;
	__int64 m_count;
	long m_bps;
	long m_channels;
	long m_chunksize;
	bool m_calcmd5;
	md5_state_t m_md5;
	int32_t m_buffer[WVSIZE];
	wchar_t m_error[MAXERROR];

	CStream m_stream;
	CStream m_stream2;
	WavpackContext* m_wpc;
	WavpackStreamReader64 m_reader;

	void CloseFiles();
	void UpdateMD5(long count);
};
