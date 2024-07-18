/*  Copyright 2004 Stephane Dallongeville
    Copyright 2004-2007 Theo Berkau
    Copyright 2006 Guillaume Duhamel

    This file is part of Yabause.

    Yabause is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Yabause is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Yabause; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "scsp.h"
#include "snd.h"
#include <gccore.h>
#include <stdlib.h>
#include <ogcsys.h>
#include <aesndlib.h>
#include <gccore.h>
#include <ogc/audio.h>
#include <ogc/cache.h>


static AESNDPB* voice = NULL;

#define NUM_BUFFERS		4
#define BUFFER_SIZE		(8*DSP_STREAMBUFFER_SIZE)

char buffers[NUM_BUFFERS][BUFFER_SIZE];
s16 stereodata16[(44100 / 60) * 16]; //11760
int playBuffer = 0;
int fillBufferOffset[NUM_BUFFERS];
int bytesBuffered = 0;
u32 fillBuffer = 0;


static void aesnd_callback(AESNDPB* voice, u32 state){
	if(state == VOICE_STATE_STREAM) {
		if(playBuffer != fillBuffer) {
			if(fillBufferOffset[playBuffer] == BUFFER_SIZE) {
				AESND_SetVoiceBuffer(voice,
						buffers[playBuffer], BUFFER_SIZE);
				playBuffer = (playBuffer + 1) % NUM_BUFFERS;
				bytesBuffered -= BUFFER_SIZE;
			}
		}
	}
}

void SNDDummySetVolume(UNUSED int volume)
{
	u16 aesnd_vol = (u16)(((float)(256.0f / 1024.0f)) * 255);
	if (voice) AESND_SetVoiceVolume(voice, aesnd_vol, aesnd_vol);
	//soundvolume = ( (double)SDL_MIX_MAXVOLUME /(double)100 ) *volume;
}

void snd_Init(void)
{
	AESND_Init();
	voice = AESND_AllocateVoice(aesnd_callback);
	AESND_SetVoiceFormat(voice, VOICE_STEREO16);
	AESND_SetVoiceFrequency(voice, 44100);
	SNDDummySetVolume(0);
	AESND_SetVoiceStream(voice, true);
	AESND_Pause(1);
}


static int SNDDummyInit(void)
{
	//fillBuffer = playBuffer = 0;
	AESND_Pause(0);
	return 0;
}

//////////////////////////////////////////////////////////////////////////////

static void SNDDummyDeInit(void)
{
	AESND_Pause(1);
}

//////////////////////////////////////////////////////////////////////////////

static int SNDDummyReset(void)
{
	return 0;
}

//////////////////////////////////////////////////////////////////////////////

static int SNDDummyChangeVideoFormat(UNUSED int vertfreq)
{
	//return (44100 / 60) * 8;
   return 0;
}


//////////////////////////////////////////////////////////////////////////////


static void snd32uto16s(s32 *src, s16 *dst, u32 len) {
	u32 i;
	for (i = 0; i < len; ++i) {
		// Are these useless?
		if (*src > 0x7FFF) *dst = 0x7FFF;
		else if (*src < -0x8000) *dst = -0x8000;
		else *dst = *src;
		++src;
		++dst;
	}
}


static void SNDDummyUpdateAudio(UNUSED u32 *leftchanbuffer, UNUSED u32 *rightchanbuffer, UNUSED u32 num_samples)
{
	char *soundData = (char *) stereodata16;
	snd32uto16s((s32*) leftchanbuffer, stereodata16, num_samples << 1);
	s32 bytesRemaining = (num_samples << 1) * sizeof(s16);

    while (bytesRemaining > 0) {
        // Compute how many bytes to copy into the fillBuffer
        int bytesToCopy = BUFFER_SIZE - fillBufferOffset[fillBuffer];
        if (bytesToCopy > bytesRemaining) {
            bytesToCopy = bytesRemaining;
        }

        // Copy the sound data into the fillBuffer
        memcpy(&buffers[fillBuffer][fillBufferOffset[fillBuffer]], soundData, bytesToCopy);
        soundData += bytesToCopy;
        bytesRemaining -= bytesToCopy;
        fillBufferOffset[fillBuffer] += bytesToCopy;
        bytesBuffered += bytesToCopy;

        // If the fillBuffer is full, advance to the next fillBuffer
        if (fillBufferOffset[fillBuffer] == BUFFER_SIZE) {
            fillBuffer = (fillBuffer + 1) % NUM_BUFFERS;
            fillBufferOffset[fillBuffer] = 0;
        }
    }

	AESND_SetVoiceStop(voice, false);
}

//////////////////////////////////////////////////////////////////////////////

static u32 SNDDummyGetAudioSpace(void)
{
   return BUFFER_SIZE - fillBufferOffset[fillBuffer];
}

//////////////////////////////////////////////////////////////////////////////

void SNDDummyMuteAudio()
{
	AESND_Pause(1);
}

//////////////////////////////////////////////////////////////////////////////

void SNDDummyUnMuteAudio()
{
	AESND_Pause(0);
}

//////////////////////////////////////////////////////////////////////////////


SoundInterface_struct SNDDummy = {
SNDCORE_DUMMY,
"Dummy Sound Interface",
SNDDummyInit,
SNDDummyDeInit,
SNDDummyReset,
SNDDummyChangeVideoFormat,
SNDDummyUpdateAudio,
SNDDummyGetAudioSpace,
SNDDummyMuteAudio,
SNDDummyUnMuteAudio,
SNDDummySetVolume
};
