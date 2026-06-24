#include "StdAfxSound.h"
#include "xmath.h"
#include "SoundInternal.h"
#include "files/files.h"
#include "Sample.h"
#include "AudioPlayer.h"

///////////////// AudioPlayer /////////////////////////////

void AudioPlayer::requestPlay(bool state) {
    request_play = state;
}

///////////////// SoundPlayer /////////////////////////////

SpeechPlayer::SpeechPlayer() = default;

SpeechPlayer::~SpeechPlayer() {
    this->destroySample();
}

void SpeechPlayer::destroySample() {
    if (sample) {
        sample->stop();
        delete sample;
        sample = nullptr;
    }
}

bool SpeechPlayer::OpenToPlay(const char* fname, bool cycled) {
    //fprintf(stderr, "SpeechPlayer %s\n", fname);
    destroySample();
    
    sample = SNDLoadSound(fname);
    if (!sample) return false;
    sample->looped = cycled; //Tecnically not need for speeches but whatever
    sample->channel_group = channel_group;
    sample->global_volume_select = global_volume_select;
    sample->volume = volume;
    sample->steal_channel = true; //Just in case another speech is playing
    
    bool playing = sample->play() != SND_NO_CHANNEL;
    if (!playing) {
        fprintf(stderr, "SpeechPlayer couldn't play %s\n", fname);
    }
    request_play = false;
    return playing;
}

void SpeechPlayer::Stop() {
    if (!sample) return;
    sample->stop();
}

void SpeechPlayer::Pause() {
    if (!sample) return;
    sample->pause();
}

void SpeechPlayer::Resume() {
    if (!sample) return;
    sample->resume();
}

bool SpeechPlayer::IsPlay() {
    if (request_play) return true;
    if (!sample) return false;
    return sample->isPlaying();
}

bool SpeechPlayer::IsPause() {
    if (!sample) return false;
    return sample->isPaused();
}

void SpeechPlayer::SetVolume(float volume_) {
    this->volume = std::max(0.0f, std::min(1.0f, volume_));
    if (!sample) return;
    sample->volume = this->volume;
    sample->updateEffects();
}

void SpeechPlayer::SetVolumeSelection(GLOBAL_VOLUME selection) {
    this->global_volume_select = selection;
    if (!sample) return;
    sample->global_volume_select = this->global_volume_select;
    sample->updateEffects();
}

GLOBAL_VOLUME SpeechPlayer::GetVolumeSelection() {
    return this->global_volume_select;
}

float SpeechPlayer::GetLen() { 
    if (!sample) return 0.0f;
    return static_cast<float>(sample->getDuration()) / 1000.0f;
}

///////////////// MusicPlayer /////////////////////////////

#ifdef PERIMETER_SDL3
static MIX_Audio* music = nullptr;

SND_Channel GetMusicChannel() {
    return SNDGetGroupAvailableChannel(SND_GROUP_MUSIC, true);
}

bool MusicPlayer::StartMusicPlay() {
    SND_Channel channel = GetMusicChannel();
    MIX_SetTrackAudio(channel, music);
    SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, loop ? -1 : 0); //Always loop or only once
    if (!MIX_PlayTrack(channel, props)) {
        fprintf(stderr, "MIX_PlayTrack music error: %s\n", SDL_GetError());
        return false;
    }
    return true;
}

#else

static Mix_Music* music = nullptr;

bool MusicPlayer::StartMusicPlay() {
    if (Mix_PlayMusic(music, loop ? -1 : 0) != -1) {
        fprintf(stderr, "Mix_PlayMusic error: %s\n", Mix_GetError());
        return false;
    }
    return true;
}

#endif

MusicPlayer::~MusicPlayer() {
    Stop();

}

bool MusicPlayer::OpenToPlay(const char* fname, bool cycled) {
    //Library not initialized
    if(!SND::has_sound_init) {
        return false;
    }

    //Stop any previous music
    this->Stop();

    std::string path = convert_path_content(fname);
    if (path.empty()) {
        return false;
    }

    loop = cycled;

#ifdef PERIMETER_SDL3
    if (!props) {
        props = SDL_CreateProperties();
    }
    if (!props) {
        SDL_Log("Couldn't create MusicPlayer props: %s", SDL_GetError());
        return false;
    }

    SND_Channel channel = GetMusicChannel();
    if (channel == SND_NO_CHANNEL) {
        fprintf(stderr, "SNDGetGroupAvailableChannel no music channel!\n");
        return false;
    }

    music = MIX_LoadAudio(SND::deviceMixer, fname, true);
    if (!music) {
        fprintf(stderr, "MIX_LoadAudio music error %s : %s\n", fname, SDL_GetError());
        return false;
    }

    if (!StartMusicPlay()) {
        Stop();
        return false;
    }
#else
    music = Mix_LoadMUS(path.c_str());
    if (!music) {
        fprintf(stderr, "Mix_LoadMUS error %s : %s\n", fname, Mix_GetError());
        return false;
    }

    if (!StartMusicPlay(loop)) {
        Stop();
        return false;
    }
#endif

    music_start_time = clockf();

    return true;
}

void MusicPlayer::Stop() {
    music_start_time = 0;
    music_pause_time = 0;
    music_faded_out_pos = 0;
    loop = false;
    if (music) {
#ifdef PERIMETER_SDL3
        SND_Channel track = GetMusicChannel();
        if (track != SND_NO_CHANNEL && MIX_TrackPlaying(track)) {
            MIX_StopTrack(track, 0);
        }
        MIX_DestroyAudio(music);

        if (props) {
            SDL_DestroyProperties(props);
            props = 0;
        }
#else
        if (Mix_PlayingMusic()) {
            Mix_HaltMusic();
        }
        Mix_FreeMusic(music);
#endif
        music = nullptr;
    }
}

void MusicPlayer::Pause() {
    if (!music) return;
#ifdef PERIMETER_SDL3
    SND_Channel channel = GetMusicChannel();
    if (channel == SND_NO_CHANNEL || !MIX_TrackPlaying(channel)) return;
    MIX_PauseTrack(channel);
#else
    if (!Mix_PlayingMusic()) return;
    Mix_PauseMusic();
#endif
    music_pause_time = clockf();
}

void MusicPlayer::Resume() {
    if (!music) return;
#ifdef PERIMETER_SDL3
    SND_Channel channel = GetMusicChannel();
    if (channel == SND_NO_CHANNEL) return;
#endif

    if (0 < music_pause_time) {
        music_start_time += clockf() - music_pause_time;
        music_pause_time = 0;
    }
    if (0 < music_faded_out_pos) {
        StartMusicPlay();
#ifdef PERIMETER_SDL3
        MIX_SetTrackPlaybackPosition(channel, MIX_TrackMSToFrames(channel, music_faded_out_pos));
#else
        Mix_SetMusicPosition(music_faded_out_pos);
#endif
        music_faded_out_pos = 0;
    } else {
#ifdef PERIMETER_SDL3
        MIX_ResumeTrack(channel);
#else
        Mix_ResumeMusic();
#endif
    }
}

bool MusicPlayer::IsPlay() {
    if (!music) return false;
#ifdef PERIMETER_SDL3
    SND_Channel track = GetMusicChannel();
    if (track == SND_NO_CHANNEL) return false;
    if (MIX_TrackPlaying(track)) return true;
#else
    if (Mix_PlayingMusic()) return true;
#endif
    return 0 < music_faded_out_pos;
}

bool MusicPlayer::IsPause() {
    if (!music) return false;
#ifdef PERIMETER_SDL3
    SND_Channel track = GetMusicChannel();
    return track == SND_NO_CHANNEL && MIX_TrackPaused(track);
#else
    return Mix_PausedMusic();
#endif
}

void MusicPlayer::SetVolume(float volume) {
    if (!SND::has_sound_init) return;
    volume = std::max(0.0f, std::min(1.0f, volume));
#ifdef PERIMETER_SDL3
    SND_Channel track = GetMusicChannel();
    if (track != SND_NO_CHANNEL) {
        MIX_SetTrackGain(track, volume);
    }
#else
    volume *= MIX_MAX_VOLUME;
    Mix_VolumeMusic(static_cast<int>(xm::round(volume)));
#endif
}

void MusicPlayer::FadeVolume(float time, float new_volume) {
    /* TODO 
       Ion: we should implement a linear volume -> new_volume while playing without using SDL_mixer as these
       stop/start the music, which doesn't happen in original code afaik but the way FadeVolume is used this should suffice
    */
    int time_ms = static_cast<int>(xm::round(time * 1000.0f));
    if (new_volume <= 0) {
        if (0 >= music_faded_out_pos && this->IsPlay()) {
#ifdef PERIMETER_SDL3
            SND_Channel channel = GetMusicChannel();
            MIX_StopTrack(channel, MIX_TrackMSToFrames(channel, time_ms));
#else
            Mix_FadeOutMusic(time_ms);
#endif
            music_faded_out_pos = (clockf() - music_start_time) / 1000.0f + time;
            music_pause_time = clockf() + time;
        }
    } else {
        SetVolume(new_volume);
        if (0 < music_faded_out_pos) {
#ifdef PERIMETER_SDL3
            int start_ms = static_cast<int>(xm::round(music_faded_out_pos * 1000.0f));
            SDL_SetNumberProperty(props, MIX_PROP_PLAY_START_MILLISECOND_NUMBER, start_ms);
            SDL_SetNumberProperty(props, MIX_PROP_PLAY_FADE_IN_MILLISECONDS_NUMBER, time_ms);
            if (!StartMusicPlay()) {
                fprintf(stderr, "FadeVolume music error: %s\n", SDL_GetError());
            }
            SDL_SetNumberProperty(props, MIX_PROP_PLAY_START_MILLISECOND_NUMBER, 0);
            SDL_SetNumberProperty(props, MIX_PROP_PLAY_FADE_IN_MILLISECONDS_NUMBER, 0);
#else
            Mix_FadeInMusicPos(music, loop ? -1 : 0, time_ms, music_faded_out_pos);
#endif
        }
        if (0 < music_pause_time) {
            music_start_time += clockf() - music_pause_time;
            music_pause_time = 0;
        }
        music_faded_out_pos = 0;
    }
}

