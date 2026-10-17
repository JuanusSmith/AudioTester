// Runs each file through the same decoder loop as AudioTester's worker threads and prints the result.
// usage: decoder_test <usemem 0|1> <file>...

#include <windows.h>
#include <stdio.h>
#include "../Decoders.h"

int wmain(int argc, wchar_t** argv)
{
	if (argc < 3)
	{
		wprintf(L"usage: decoder_test <usemem 0|1> <file>...\n");
		return 2;
	}
	CDecoder::m_usemem = argv[1][0] == L'1';
	CDecoders decoders;
	for (int i = 2; i < argc; i++)
	{
		const wchar_t* name = wcsrchr(argv[i], L'\\');
		name = name ? name + 1 : argv[i];
		if (!decoders.IsSupportedType(argv[i]))
		{
			wprintf(L"%s\tSKIPPED\n", name);
			continue;
		}
		CDecoder* decoder = decoders.OpenDecoder(argv[i]);
		if (!decoder)
		{
			wprintf(L"%s\tFAIL\tUNABLE TO OPEN DECODER\n", name);
			continue;
		}
		long count;
		while ((count = decoder->Read()) > 0)
			;
		if (count < 0)
			wprintf(L"%s\tFAIL\t%s\n", name, decoder->GetLastError());
		else
			wprintf(L"%s\tPASS\n", name);
		delete decoder;
	}
	return 0;
}
