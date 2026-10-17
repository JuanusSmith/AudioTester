#pragma once

#include "decoders.h"
#include "stream.h"

const wchar_t MP3TYPES[]=L"mp3\0mp2\0m2a\0";
const long MP3_BITRATES[16][5]={{0,0,0,0,0},{32,32,32,32,8},{64,48,40,48,16},{96,56,48,56,24},{128,64,56,64,32},{160,80,64,80,40},{192,96,80,96,48},{224,112,96,112,56},{256,128,112,128,64},{288,160,128,144,80},{320,192,160,160,96},{352,224,192,176,112},{384,256,224,192,128},{416,320,256,224,144},{448,384,320,256,160},{0,0,0,0,0}};
const long MP3_SAMPRATES[4][3]={{44100,22050,11025},{48000,24000,12000},{32000,16000,8000},{0,0,0}};
const long MP3_CRCBYTES[2][2]={{32,17},{17,9}};	// http://www.codeproject.com/audio/MPEGAudioInfo.asp#CRC

#define MPEG1		3
#define MPEG2		2
#define MPEG2_5		0
#define LAYER1		3
#define LAYER2		2
#define LAYER3		1

#define MAXRESYNC	65536

#define APE_TAG_FOOTER_BYTES					32
#define APE_TAG_FOOTER_ID						"APETAGEX"
#define APE_TAG_FLAG_CONTAINS_HEADER            (1 << 31)

typedef struct
{
    char ID[8];              // should equal 'APETAGEX'    
    int Version;             // equals CURRENT_APE_TAG_VERSION
    int Size;                // the complete size of the tag, including this footer (excludes header)
    int Fields;              // the number of fields in the tag
    int Flags;               // the tag flags
    char Reserved[8];        // reserved for later use (must be zero)
}APE_TAG_FOOTER;

class CMP3Decoder :	public CDecoder
{
public:
	CMP3Decoder();
	CMP3Decoder(const wchar_t* filename);
	virtual ~CMP3Decoder();

	virtual const wchar_t* GetSupportedTypes();
	virtual CDecoder* NewDecoder(const wchar_t* filename);

	virtual __int64 GetSampleCount();
	virtual const wchar_t* GetLastError();
	virtual long Read();

	bool m_opened;

private:
	long m_rate;
	wchar_t m_error[MAXERROR];
	unsigned long m_header;
	unsigned long m_lastheader;
	CStream m_stream;
	__int64 m_filesize;
	__int64 m_offset;
	__int64 m_samplepos;
	int m_version;
	int m_layer;
	int m_bitrate;
	int m_samplerate;
	int m_padding;
	int m_mono;
	int m_column;
	int m_headerlength;
	int m_footerlength;
	int m_framelength;
	int m_resync;
	int m_crcbytes;
	unsigned short m_crc;
	unsigned char m_crcbuf[40];
	int GetHeaderLength();
	int GetFooterLength();
	int GetLyricsLength();
	int GetFrameLength();
	bool Resync();
	bool CheckCRC();
	unsigned short CRC16(int crc, unsigned char* buf, int length);
};
