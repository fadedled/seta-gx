/*
 * snd.h - Sound output
 */

#ifndef __SOUND_H__
#define __SOUND_H__

void snd_Init(void);
void snd_SetVolume(int volume);
void snd_DeInit(void);
int snd_Reset(void);
int snd_ChangeVideoFormat(int vertfreq);
void snd_UpdateAudio(u32 *leftchanbuffer, u32 *rightchanbuffer, u32 num_samples);
u32 snd_GetAudioSpace(void);
void snd_MuteAudio();
void snd_UnMuteAudio();


#endif /* __SOUND_H__ */
