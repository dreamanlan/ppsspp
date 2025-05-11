// Copyright (c) 2012- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#pragma once

#include <map>

class PointerWrap;

// audioType
enum PSPAudioType {
	PSP_CODEC_AT3PLUS = 0x00001000,
	PSP_CODEC_AT3 = 0x00001001,
	PSP_CODEC_MP3 = 0x00001002,
	PSP_CODEC_AAC = 0x00001003,  // sceMp4 decodes this in an mp4 container
	PSP_CODEC_WMA = 0x00001005,
};

struct SceAudiocodecCodec {
	s32 unk_init;
	s32 unk4;
	s32 err; // 8
	s32 edramAddr; // c  // presumably in ME memory?
	s32 neededMem; // 10  // 0x102400 for Atrac3+
	s32 inited;  // 14
	u32 inBuf; // 18  // Before decoding, set this to the start of the raw frame.
	s32 srcBytesRead; // 1c
	u32 outBuf; // 20  // This is where the decoded data is written.
	s32 dstSamplesWritten; // 24
	// Probably, from here on out is a union with different fields for different codecs.
	union {  // offset 40 / 0x28
		struct {
			s8 unk40;  // 28  format or looping related
			s8 unk41;  // 29  format or looping related
			s8 unk42;
			s8 unk43;
		};
		u32 formatOutSamples;
	};
	union {  // offset 44 / 0x2C
		struct {
			u8 unk44;
			s8 unk45;
			s8 unk46;
			s8 unk47;
		};
		struct {
			s16 unk44_16;
			s16 unk46_16;
		};
		u32 unk44_32;
	};
	s32 unk48;  // 30 Atrac3 (non-+) related. Zero with Atrac3+.
	s32 unk52;  // 34
	s32 mp3_9999;  // 38  // unk56
	s32 mp3_3;  // unk60 gets the value 3
	s32 unk64;  // Atrac3+ size related
	s32 mp3_9;
	s32 mp3_0;
	s32 unk76;
	s32 unk80;
	s32 mp3_1_first;
	s32 unk88;
	s32 unk92;
	s32 mp3_1;
	s32 unk100;
	u32 allocMem; // 104
	// make sure the size is 128
	u8 unk[20];
};

void __AudioCodecInit();
void __AudioCodecShutdown();
void Register_sceAudiocodec();
void __sceAudiocodecDoState(PointerWrap &p);

class AudioDecoder;
extern std::map<u32, AudioDecoder *> g_audioDecoderContexts;

bool IsAtrac3StreamJointStereo(int codecType, int bytesPerFrame, int channels);
