#pragma once

#include "decoders.h"
#include "stream.h"
#include <stdio.h>
#include <FLAC++/all.h>

const wchar_t FLACTYPES[]=L"flac\0fla\0";

class CFlacDecoder : public CDecoder, public FLAC::Decoder::Stream
{
public:
	CFlacDecoder();
	CFlacDecoder(const wchar_t* filename);
	virtual ~CFlacDecoder();

	virtual const wchar_t* GetSupportedTypes();
	virtual CDecoder* NewDecoder(const wchar_t* filename);

	virtual __int64 GetSampleCount();
	virtual const wchar_t* GetLastError();
	virtual long Read();

	virtual FLAC__StreamDecoderReadStatus read_callback(FLAC__byte [],size_t *);
	virtual FLAC__StreamDecoderSeekStatus seek_callback(FLAC__uint64);
	virtual FLAC__StreamDecoderTellStatus tell_callback(FLAC__uint64 *);
	virtual FLAC__StreamDecoderLengthStatus length_callback(FLAC__uint64 *);
	virtual bool eof_callback();
	virtual FLAC__StreamDecoderWriteStatus write_callback(const FLAC__Frame *,const FLAC__int32 *const []);
	virtual void metadata_callback(const FLAC__StreamMetadata *);
	virtual void error_callback(FLAC__StreamDecoderErrorStatus);

	bool m_opened;

private:
	__int64 m_samples;
	__int64 m_count;
	long m_rate;
	bool m_error;
	wchar_t m_str[MAXERROR];

	CStream m_stream;
	long m_framesize;

	void Truncated();
};
