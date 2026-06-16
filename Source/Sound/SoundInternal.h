#pragma once
#include "PerimeterSound.h"

#define SAFE_DELETE(p)  { if(p) { delete (p);     (p)=NULL; } }

void SNDSetupChannelCallback(int mixChannels, bool init);
void SNDUpdateAllSoundVolume();
class SND_Sample;
SND_Sample* SNDLoadSound(const std::string& fname);
void SNDSetChannelBusy(int channel, bool playing);
int SNDGetChannelIndex(SND_Channel channel);
SND_Channel SNDGetChannelFromIndex(int channelIdx);
SND_Channel SNDGetGroupAvailableChannel(int group, bool steal);

namespace SND {
extern size_t totalMixChannels;
extern float sound_volume;
extern float voice_volume;
extern bool has_sound_init;
extern int deviceFrequency;
extern int deviceChannels;
extern SDL_AudioFormat deviceFormat;
#ifdef PERIMETER_SDL3
extern SDL_PropertiesID props_track_looped = 0;
extern MIX_Mixer* deviceMixer;
#endif
///Minimum required volume to be played, this avoids starving channels by too many low volume effects
const float EFFECT_VOLUME_THRESHOLD = 0.05;
void logs(const char *format, ...);
};
