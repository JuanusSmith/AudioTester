#include "Decoders.h"
#include "MP3Decoder.h"
#include "OggDecoder.h"
#include "FlacDecoder.h"
#include "WvDecoder.h"
#include <wctype.h>

using namespace std;

bool CDecoder::m_usemem=0;

CDecoders::CDecoders()
{
	AddDecoder(new CMP3Decoder);
	AddDecoder(new COggDecoder);
	AddDecoder(new CFlacDecoder);
	AddDecoder(new CWvDecoder);
}

CDecoders::~CDecoders()
{
	for(list<CDecoder*>::const_iterator i=m_list.begin();i!=m_list.end();i++)
		delete *i;
}

bool CDecoders::IsSupportedType(const wchar_t* filename)
{
	if(FindDecoder(filename))
		return 1;
	else
		return 0;
}

CDecoder* CDecoders::OpenDecoder(const wchar_t* filename)
{
	CDecoder* d=FindDecoder(filename);
	if(d)
		return d->NewDecoder(filename);
	else
		return 0;
}

void CDecoders::AddDecoder(CDecoder* decoder)
{
	m_list.push_back(decoder);
	const wchar_t* a=decoder->GetSupportedTypes();
	while(*a)
	{
		m_map[a]=decoder;
		a+=wcslen(a)+1;
	}
}

CDecoder* CDecoders::FindDecoder(const wchar_t* filename)
{
	const wchar_t* a=wcsrchr(filename,'.');
	if(!a++)
		return 0;
	wstring ext(a);
	for(size_t i=0;i<ext.size();i++)
		ext[i]=towlower(ext[i]);
	map<wstring,CDecoder*>::const_iterator i=m_map.find(ext);
	return (i!=m_map.end())?(*i).second:0;
}
