// Writes known-good test files for each supported format into the folder given on the command line.
// Damaged copies are made from these by make_damaged_files.py.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string>
#include <vector>
#include <FLAC/stream_encoder.h>
#include <vorbis/vorbisenc.h>
#include <wavpack/wavpack.h>
#include "../md5.h"

using namespace std;

static const int RATE = 44100;
static const int CHANNELS = 2;
static const int FRAMES = RATE * 10;	// 10 seconds

static vector<int> MakeSine()
{
	vector<int> samples(FRAMES * CHANNELS);
	for (int i = 0; i < FRAMES; i++)
	{
		samples[i * 2] = (int)(12000 * sin(2 * 3.14159265 * 440 * i / RATE));
		samples[i * 2 + 1] = (int)(12000 * sin(2 * 3.14159265 * 660 * i / RATE));
	}
	return samples;
}

static bool WriteFlac(const string& path, const vector<int>& samples)
{
	FLAC__StreamEncoder* enc = FLAC__stream_encoder_new();
	FLAC__stream_encoder_set_channels(enc, CHANNELS);
	FLAC__stream_encoder_set_bits_per_sample(enc, 16);
	FLAC__stream_encoder_set_sample_rate(enc, RATE);
	FLAC__stream_encoder_set_total_samples_estimate(enc, FRAMES);
	bool ok = FLAC__stream_encoder_init_file(enc, path.c_str(), 0, 0) == FLAC__STREAM_ENCODER_INIT_STATUS_OK;
	ok = ok && FLAC__stream_encoder_process_interleaved(enc, samples.data(), FRAMES);
	ok = FLAC__stream_encoder_finish(enc) && ok;
	FLAC__stream_encoder_delete(enc);
	return ok;
}

// Without seek/tell callbacks the encoder can't go back and fill in STREAMINFO,
// so the total sample count stays 0 (unknown) and the MD5 stays blank
static FLAC__StreamEncoderWriteStatus FlacWrite(const FLAC__StreamEncoder*, const FLAC__byte buffer[], size_t bytes, uint32_t, uint32_t, void* client)
{
	return fwrite(buffer, 1, bytes, (FILE*)client) == bytes ? FLAC__STREAM_ENCODER_WRITE_STATUS_OK : FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
}

static bool WriteFlacUnknownLength(const string& path, const vector<int>& samples)
{
	FILE* f = fopen(path.c_str(), "wb");
	if (!f)
		return false;
	FLAC__StreamEncoder* enc = FLAC__stream_encoder_new();
	FLAC__stream_encoder_set_channels(enc, CHANNELS);
	FLAC__stream_encoder_set_bits_per_sample(enc, 16);
	FLAC__stream_encoder_set_sample_rate(enc, RATE);
	bool ok = FLAC__stream_encoder_init_stream(enc, FlacWrite, 0, 0, 0, f) == FLAC__STREAM_ENCODER_INIT_STATUS_OK;
	ok = ok && FLAC__stream_encoder_process_interleaved(enc, samples.data(), FRAMES);
	ok = FLAC__stream_encoder_finish(enc) && ok;
	FLAC__stream_encoder_delete(enc);
	fclose(f);
	return ok;
}

static bool WriteVorbis(const string& path, const vector<int>& samples)
{
	FILE* f = fopen(path.c_str(), "wb");
	if (!f)
		return false;
	vorbis_info vi;
	vorbis_comment vc;
	vorbis_dsp_state vd;
	vorbis_block vb;
	ogg_stream_state os;
	ogg_page og;
	ogg_packet op;
	vorbis_info_init(&vi);
	if (vorbis_encode_init_vbr(&vi, CHANNELS, RATE, 0.4f))
		return false;
	vorbis_comment_init(&vc);
	vorbis_analysis_init(&vd, &vi);
	vorbis_block_init(&vd, &vb);
	ogg_stream_init(&os, 1234);
	ogg_packet header, header_comm, header_code;
	vorbis_analysis_headerout(&vd, &vc, &header, &header_comm, &header_code);
	ogg_stream_packetin(&os, &header);
	ogg_stream_packetin(&os, &header_comm);
	ogg_stream_packetin(&os, &header_code);
	while (ogg_stream_flush(&os, &og))
	{
		fwrite(og.header, 1, og.header_len, f);
		fwrite(og.body, 1, og.body_len, f);
	}
	const int CHUNK = 1024;
	int pos = 0;
	bool eos = false;
	while (!eos)
	{
		int n = (FRAMES - pos < CHUNK) ? FRAMES - pos : CHUNK;
		if (n > 0)
		{
			float** buf = vorbis_analysis_buffer(&vd, n);
			for (int i = 0; i < n; i++)
				for (int c = 0; c < CHANNELS; c++)
					buf[c][i] = samples[(pos + i) * CHANNELS + c] / 32768.0f;
			vorbis_analysis_wrote(&vd, n);
			pos += n;
		}
		else
			vorbis_analysis_wrote(&vd, 0);
		while (vorbis_analysis_blockout(&vd, &vb) == 1)
		{
			vorbis_analysis(&vb, 0);
			vorbis_bitrate_addblock(&vb);
			while (vorbis_bitrate_flushpacket(&vd, &op))
			{
				ogg_stream_packetin(&os, &op);
				while (!eos && ogg_stream_pageout(&os, &og))
				{
					fwrite(og.header, 1, og.header_len, f);
					fwrite(og.body, 1, og.body_len, f);
					if (ogg_page_eos(&og))
						eos = true;
				}
			}
		}
	}
	ogg_stream_clear(&os);
	vorbis_block_clear(&vb);
	vorbis_dsp_clear(&vd);
	vorbis_comment_clear(&vc);
	vorbis_info_clear(&vi);
	fclose(f);
	return true;
}

static int WvWrite(void* id, void* data, int32_t bcount)
{
	return fwrite(data, 1, bcount, (FILE*)id) == (size_t)bcount;
}

static bool WriteWavPack(const string& path, vector<int> samples)
{
	FILE* f = fopen(path.c_str(), "wb");
	if (!f)
		return false;
	WavpackContext* wpc = WavpackOpenFileOutput(WvWrite, f, 0);
	WavpackConfig cfg;
	memset(&cfg, 0, sizeof(cfg));
	cfg.bits_per_sample = 16;
	cfg.bytes_per_sample = 2;
	cfg.num_channels = CHANNELS;
	cfg.channel_mask = 3;
	cfg.sample_rate = RATE;
	cfg.flags = CONFIG_MD5_CHECKSUM;
	bool ok = WavpackSetConfiguration64(wpc, &cfg, FRAMES, 0) && WavpackPackInit(wpc);
	// MD5 of the little-endian PCM, as WavPack's own encoder stores it
	md5_state_t md5;
	md5_init(&md5);
	for (size_t i = 0; i < samples.size(); i++)
	{
		unsigned char b[2] = { (unsigned char)samples[i], (unsigned char)(samples[i] >> 8) };
		md5_append(&md5, b, 2);
	}
	unsigned char digest[16];
	md5_finish(&md5, digest);
	ok = ok && WavpackPackSamples(wpc, (int32_t*)samples.data(), FRAMES);
	ok = ok && WavpackFlushSamples(wpc);
	ok = ok && WavpackStoreMD5Sum(wpc, digest) && WavpackFlushSamples(wpc);
	WavpackCloseFile(wpc);
	fclose(f);
	return ok;
}

static unsigned short Crc16(int crc, const unsigned char* buf, int length)	// Same as CMP3Decoder::CRC16
{
	for (int i = 0; i < length; i++)
	{
		int k = buf[i] << 8;
		for (int j = 0; j < 8; j++)
		{
			k <<= 1;
			crc <<= 1;
			if ((crc ^ k) & 0x10000)
				crc ^= 0x8005;
		}
	}
	return crc & 0xffff;
}

// MPEG-1 Layer III, 128kbps, 44.1kHz, stereo frames with an empty payload. AudioTester checks
// frame structure, sync and CRCs rather than decoding audio, so silence is enough.
static bool WriteMp3(const string& path, bool crc, int id3size)
{
	FILE* f = fopen(path.c_str(), "wb");
	if (!f)
		return false;
	if (id3size)
	{
		unsigned char tag[10] = { 'I', 'D', '3', 3, 0, 0,
			(unsigned char)((id3size >> 21) & 0x7f), (unsigned char)((id3size >> 14) & 0x7f),
			(unsigned char)((id3size >> 7) & 0x7f), (unsigned char)(id3size & 0x7f) };
		fwrite(tag, 1, 10, f);
		vector<unsigned char> body(id3size, 0);
		fwrite(body.data(), 1, body.size(), f);
	}
	const int FRAMELEN = 417;	// 144*128000/44100, no padding
	for (int n = 0; n < 400; n++)
	{
		unsigned char frame[FRAMELEN];
		memset(frame, 0, sizeof(frame));
		frame[0] = 0xff;
		frame[1] = crc ? 0xfa : 0xfb;
		frame[2] = 0x90;
		frame[3] = 0x00;
		if (crc)
		{
			// Side info (32 bytes for MPEG-1 stereo) gets a non-zero pattern so damage to it is detectable
			for (int i = 0; i < 32; i++)
				frame[6 + i] = (unsigned char)(i * 7 + n);
			frame[6] = 0;	// main_data_begin high bits, so no bit reservoir is referenced
			unsigned char crcbuf[34];
			crcbuf[0] = frame[2];
			crcbuf[1] = frame[3];
			memcpy(crcbuf + 2, frame + 6, 32);
			unsigned short c = Crc16(0xffff, crcbuf, 34);
			frame[4] = (unsigned char)(c >> 8);
			frame[5] = (unsigned char)c;
		}
		fwrite(frame, 1, sizeof(frame), f);
	}
	fclose(f);
	return true;
}

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		printf("usage: make_test_files <folder>\n");
		return 2;
	}
	string dir = argv[1];
	if (dir.back() != '\\' && dir.back() != '/')
		dir += '\\';
	vector<int> samples = MakeSine();
	struct { const char* name; bool ok; } results[] = {
		{ "good.flac", WriteFlac(dir + "good.flac", samples) },
		{ "good_unknown_length.flac", WriteFlacUnknownLength(dir + "good_unknown_length.flac", samples) },
		{ "good.ogg", WriteVorbis(dir + "good.ogg", samples) },
		{ "good.wv", WriteWavPack(dir + "good.wv", samples) },
		{ "good.mp3", WriteMp3(dir + "good.mp3", false, 0) },
		{ "good_crc.mp3", WriteMp3(dir + "good_crc.mp3", true, 0) },
		{ "good_id3v2.mp3", WriteMp3(dir + "good_id3v2.mp3", false, 512) },
	};
	int failures = 0;
	for (auto& r : results)
	{
		printf("%-28s %s\n", r.name, r.ok ? "written" : "FAILED");
		failures += !r.ok;
	}
	return failures;
}
