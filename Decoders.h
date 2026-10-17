#pragma once

class CDecoder
{
public:
	CDecoder(){}
	virtual ~CDecoder(){}

	static bool m_usemem;	// Use a memory buffer for decoders

	virtual const wchar_t* GetSupportedTypes()=0;	// Returns an array of null separated file extensions, terminated by double null
	virtual CDecoder* NewDecoder(const wchar_t* filename)=0;	

	virtual __int64 GetSampleCount()=0;
	virtual const wchar_t* GetLastError()=0;
	virtual long Read()=0; // Returns the number of samples read (0 on end of file, -1 on error)
};

#define MAXERROR	256

#include <list>
#include <map>
#include <string>
using namespace std;

class CDecoders
{
public:
	CDecoders();
	virtual ~CDecoders();
	bool IsSupportedType(const wchar_t* filename);
	CDecoder* OpenDecoder(const wchar_t* filename);
private:
	list<CDecoder*> m_list;
	map<wstring,CDecoder*> m_map;
	void AddDecoder(CDecoder* decoder);
	CDecoder* FindDecoder(const wchar_t* filename);
};
