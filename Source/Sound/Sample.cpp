#include "StdAfxSound.h"
#include "SoundInternal.h"
#include "Sample.h"
#include "../Render/inc/RenderMT.h"

//Called when channel is finished
#ifdef PERIMETER_SDL3
static void callbackTrackStopped(void *userdata, MIX_Track *track) {
    SND_Sample* sample = reinterpret_cast<SND_Sample*>(userdata);
    printf("Channel %p finished playing %p.\n", track);
    if (sample->track == track) {
        sample->track = nullptr;
    }
}
#else // PERIMETER_SDL3

///Used for tracking what channel is playing what sample, if sample is not here then is not being played
std::vector<SND_Sample*> channelSamples;

///Avoid channelSamples being accessed by callback while iterating/modifying
MTSection channelSamplesLock;

static void callbackChannelFinished(int channel)
{
    //printf("Channel %d finished playing.\n", channel);
    MTAuto mtenter(&channelSamplesLock);
    if (channel < channelSamples.size()) {
        channelSamples[channel] = nullptr;
    }
}
#endif // PERIMETER_SDL3

void SNDSetupChannelCallback(int mixChannels, bool init) {
#ifndef PERIMETER_SDL3
    Mix_ChannelFinished(init ? callbackChannelFinished : nullptr);
    if (init) {
        channelSamples.resize(mixChannels, nullptr);
    } else {
        channelSamples.clear();
    }
#endif
}

MixChunkWrapper::~MixChunkWrapper() {
    if (chunk) {
#ifdef PERIMETER_SDL3
        MIX_DestroyAudio(chunk);
#else
        //Only free with Mix if sound is inited, check Mix_Init just in case
        if (SND::has_sound_init || (Mix_Init(0) != 0)) {
            Mix_FreeChunk(chunk);
        } else {
            SDL_free(chunk);
        }
#endif
        chunk = nullptr;
    }
}

SND_Sample::SND_Sample(const std::shared_ptr<MixChunkWrapper>& chunk)  {
    this->chunk = chunk;
    this->millis = 0;
    SND_Chunk* current = getChunk();
#ifdef PERIMETER_SDL3
    if (current) {
        this->millis = MIX_AudioFramesToMS(current, MIX_GetAudioDuration(current));
    }
#else
    this->chunk_source = chunk;
    if (current) {
        this->millis = SNDcomputeAudioLengthMS(current->alen);
    }
#endif
}

SND_Sample::~SND_Sample() {
#ifdef PERIMETER_SDL3
    if (track) {
        MIX_StopTrack(track);
        track = nullptr;
    }
#endif

    chunk_source = nullptr;
    chunk = nullptr;
}

bool SND_Sample::loadRawData(uint8_t* src_data, size_t src_len, bool copy, const std::string& file_name) {
#ifdef PERIMETER_SDL3
    SDL_AudioSpec spec = { SND::deviceFormat, SND::deviceChannels, SND::deviceFrequency };
    SND_Chunk* new_chunk = MIX_LoadRawAudio(SND::deviceMixer, src_data, src_len, &spec);
    chunk = std::make_shared<MixChunkWrapper>(new_chunk, file_name);
    this->millis = MIX_AudioFramesToMS(new_chunk, MIX_GetAudioDuration(new_chunk));
#else
    Mix_Chunk* new_chunk = (Mix_Chunk*) SDL_malloc(sizeof(Mix_Chunk));
    new_chunk->allocated = 1;
    if (copy) {
        new_chunk->abuf = (Uint8*) SDL_calloc(1, src_len);
        SDL_memcpy(new_chunk->abuf, src_data, src_len);
    } else {
        new_chunk->abuf = src_data;
    }
    new_chunk->alen = src_len;
    if (chunk_source) {
        new_chunk->volume = chunk_source->chunk->volume;
    } else {
        new_chunk->volume = 128;
    }
    chunk_source = chunk = std::make_shared<MixChunkWrapper>(new_chunk, file_name);
    this->millis = SNDcomputeAudioLengthMS(src_len);
    this->chunk_frequency = this->frequency;
#endif
    return true;
}

SND_Channel SND_Sample::play() {
    SND_Channel channel = getChannel();
    //Don't play if already playing
    if (channel != SND_NO_CHANNEL) {
        return SND_NO_CHANNEL;
    }
    
    //Print using channels
    //fprintf(stderr, "Playing: %d\n", Mix_Playing(-1));
    
    //Find a available channel
    if (channel_group == SND_NO_CHANNEL_GROUP) {
        fprintf(stderr, "SND_Sample channel_group not set!\n");
        channel_group = SND_GROUP_EFFECTS;
    }
    channel = SNDGetGroupAvailableChannel(channel_group, false);
    if (channel == SND_NO_CHANNEL && channel_group == SND_GROUP_EFFECTS) {
        //Avoid playing if volume is too low since we are starved already
        if (this->volume < SND::EFFECT_VOLUME_THRESHOLD) {
            return SND_NO_CHANNEL;
        }
        
        //Use the specific groups
        channel = SNDGetGroupAvailableChannel(this->looped ? SND_GROUP_EFFECTS_LOOPED : SND_GROUP_EFFECTS_ONCE, false);
    }
    
    //Steal if necessary
    if (channel == SND_NO_CHANNEL && steal_channel) {
        channel = SNDGetGroupAvailableChannel(this->channel_group, true);
        if (channel != SND_NO_CHANNEL) {
#ifdef PERIMETER_SDL3
            MIX_StopTrack(channel, 0);
#else
            Mix_HaltChannel(channel);
#endif
        }
    }
    
    //Play if found
    if (channel != SND_NO_CHANNEL) {
        //Setup frequency
        this->frequency = std::max(0.0f, std::min(50.0f, this->frequency));
#ifndef PERIMETER_SDL3
        if (needFrequencyChange()) {
            this->convertChunkFrequency();
        }
#endif
        
        //Setup channel that is going to play this sound
        this->updateEffects(channel);

        //Play audio sample
        SND_Chunk* chunk_play = getChunk();
#ifndef PERIMETER_SDL3
        chunk_play->volume = getChunkSource()->volume;
#endif
        bool loop = this->external_looped_restart ? false : this->looped; //Set loop flag if not externally controlled
#ifdef PERIMETER_SDL3
        MIX_SetTrackStoppedCallback(channel, callbackTrackStopped, this);
        MIX_SetTrackAudio(channel, chunk_play);
        MIX_PlayTrack(channel, SND::props_track_looped);
#else
        channel = Mix_PlayChannel(channel, chunk_play, loop ? -1 : 0);
        if (channel == -1) { //Return's -1 if fails to play
            fprintf(stderr, "Mix_PlayChannel error (%s): %s\n",  chunk->fileName.c_str(), Mix_GetError());
            channel = SND_NO_CHANNEL;
        }

        if (channel != SND_NO_CHANNEL) {
            //Store channel for callback
            MTAuto mtenter(&channelSamplesLock);
            int channelIndex = SNDGetChannelIndex(channel);
            xassert(channelIndex < channelSamples.size());
            xassert(channelSamples[channelIndex] == nullptr);
            channelSamples[channelIndex] = this;
        }
#endif
    }

    return channel;
}

bool SND_Sample::updateEffects(SND_Channel channel) {
    bool updated = false;
#ifdef PERIMETER_SDL3
    if (getChunk() != MIX_GetTrackAudio(channel)) {
        //This means the track changed audio but we weren't notified properly
        track = nullptr;
        return false;
    }
#endif
    if (channel != SND_NO_CHANNEL) {
        //Setup volume
        this->volume = std::max(0.0f, std::min(1.0f, this->volume));
        float global_vol = 1.0f;
        switch (global_volume_select) {
            default:
            case GLOBAL_VOLUME_IGNORE:
                break;
            case GLOBAL_VOLUME_CHANNEL:
                global_vol = channel_group == SND_GROUP_SPEECH ? 1.0f : SND::sound_volume;
                break;
            case GLOBAL_VOLUME_VOICE:
                global_vol = SND::voice_volume;
                break;
            case GLOBAL_VOLUME_EFFECTS:
                global_vol = SND::sound_volume;
                break;
        }

        //Setup panning
        this->pan = std::max(0.0f, std::min(1.0f, this->pan));
#ifdef PERIMETER_SDL3
        MIX_SetTrackFrequencyRatio(channel, this->frequency);
        MIX_SetTrackGain(channel, this->volume * global_vol);
        MIX_StereoGains stereo = { 1.0f - this->pan, this->pan};
        MIX_SetTrackStereo(channel, &stereo);
#else
        Mix_Volume(channel, static_cast<int>(128.0f * this->volume * global_vol));

        int right = static_cast<int>(255 * this->pan);
        Mix_SetPanning(channel, 255 - right, right);
#endif
        
        updated = true;
    }
    return updated;
}

bool SND_Sample::updateEffects() {
    SND_Channel channel = getChannel();

#ifndef PERIMETER_SDL3
    //Check if frequency changed 
    if (channel != SND_NO_CHANNEL && needFrequencyChange()) {
        //Okay this sound is not gonna stop so we need to stop first to update frequency at play
        if (this->looped && !this->external_looped_restart) {
            if (stop()) {
                //We need to restart manually
                //fprintf(stderr, "%p Frequency changed %f -> %f\n", this, this->chunk_frequency, this->frequency);
                return play() != SND_NO_CHANNEL;
            }
        }
    }
#endif

    return updateEffects(channel);
}

SND_Channel SND_Sample::getChannel() const {
    if (!SND::has_sound_init) return SND_NO_CHANNEL;
#ifdef PERIMETER_SDL3
    return track;
#else
    MTAuto mtenter(&channelSamplesLock);
    SND_Channel channel = SND_NO_CHANNEL;
    for (int i = 0; i < channelSamples.size(); ++i) {
        if (channelSamples[i] == this) {
            channel = SNDGetChannelFromIndex(i);
            break;
        }
    }
    return channel;
#endif
}

bool SND_Sample::isPlaying() const {
    SND_Channel channel = getChannel();
    return channel != SND_NO_CHANNEL;
}

bool SND_Sample::isPaused() const {
    SND_Channel channel = getChannel();
    if (channel == SND_NO_CHANNEL) {
        return false;
    }
#ifdef PERIMETER_SDL3
    return MIX_TrackPaused(channel);
#else
    return Mix_Paused(channel);
#endif
}

bool SND_Sample::pause() const {
    SND_Channel channel = getChannel();
    if (channel == SND_NO_CHANNEL) {
        return false;
    }
#ifdef PERIMETER_SDL3
    return MIX_PauseTrack(channel);
#else
    return Mix_Pause(channel);
#endif
}

bool SND_Sample::resume() const {
    SND_Channel channel = getChannel();
    if (channel == SND_NO_CHANNEL) {
        return false;
    }
#ifdef PERIMETER_SDL3
    return MIX_ResumeTrack(channel);
#else
    return Mix_Resume(channel);
#endif
}

bool SND_Sample::stop() const {
    SND_Channel channel = getChannel();
    if (channel == SND_NO_CHANNEL) {
        return false;
    }
#ifdef PERIMETER_SDL3
    return MIX_StopTrack(channel, 0);
#else
    return Mix_HaltChannel(channel);
#endif
}

#ifndef PERIMETER_SDL3
bool SND_Sample::needFrequencyChange() const {
    return std::abs(this->frequency - this->chunk_frequency) > 0.10;
}

bool SND_Sample::convertChunkFrequency() {
    int new_frequency = static_cast<int>(static_cast<float>(SND::deviceFrequency) * this->frequency);
    if (SND::deviceFrequency == new_frequency) {
        return false;
    }

    SDL_AudioCVT cvt;
    int res = SDL_BuildAudioCVT(
            &cvt,
            //Source chunk format
            SND::deviceFormat, SND::deviceChannels, SND::deviceFrequency,
            //Destination format, we basically want new frequency
            SND::deviceFormat, SND::deviceChannels, new_frequency
    );
    if (res < 0) {
        SDL_PRINT_ERROR("SND_Sample SDL_BuildAudioCVT");
    } else if (cvt.needed) {
        Mix_Chunk* source = getChunkSource();
        if (!source) {
            return false;
        }

        //We need to allocate buffer for conversion
        cvt.len = static_cast<int>(source->alen);
        cvt.buf = (Uint8*) SDL_calloc(1, cvt.len * cvt.len_mult);
        if (cvt.buf == nullptr) {
            ErrH.Abort("Out of memory");
        }

        //Copy audio data to buffer
        SDL_memcpy(cvt.buf, source->abuf, cvt.len);

        //Convert the audio and replace old chunk audio with converted data
        if (SDL_ConvertAudio(&cvt) < 0) {
            SDL_PRINT_ERROR("SND_Sample SDL_BuildAudioCVT");
        } else {
            Mix_Chunk* new_chunk = (Mix_Chunk *) SDL_malloc(sizeof(Mix_Chunk));
            new_chunk->allocated = 1;
            new_chunk->volume = source->volume;
            new_chunk->abuf = cvt.buf;
            new_chunk->alen = cvt.len_cvt;
            chunk = std::make_shared<MixChunkWrapper>(new_chunk, source->fileName);
            this->chunk_frequency = this->frequency;
            return true;
        }
    }
    return false;
}
#endif
