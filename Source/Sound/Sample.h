#ifndef PERIMETER_SAMPLE_H
#define PERIMETER_SAMPLE_H

#include "SampleParams.h"

#ifdef PERIMETER_SDL3
using SND_Chunk = MIX_Audio;
#else
using SND_Chunk = Mix_Chunk;
#endif


//Wrapper for chunk that will be freed once unused
class MixChunkWrapper {
public:
    SND_Chunk* chunk = nullptr;
    const std::string fileName = {};
    explicit MixChunkWrapper(SND_Chunk* chunk_, const std::string& fileName_) : chunk(chunk_), fileName(fileName_) {}
    ~MixChunkWrapper();
};

//Wrapper to store certain data that will be user for mixer channels
class SND_Sample {
public:    
    float volume = 1.0f;
    float frequency = 1.0f;
    float pan = 0.5f; //0.0 left 0.5 center 1.0 right
    ///Will this sample auto start if is stopped and is looped? this is useful for 3d audios that need frequency updating
    ///so we can just let it handle the restart if we stop
    bool external_looped_restart = false;
    bool looped = false;
    ///Steals the oldest playing channel in group if none were found
    bool steal_channel = false;
    ///Group to use for this sample
    int channel_group = SND_NO_CHANNEL_GROUP;
    ///Which global volume source to use
    GLOBAL_VOLUME global_volume_select = GLOBAL_VOLUME_CHANNEL;

    explicit SND_Sample(const std::shared_ptr<MixChunkWrapper>& chunk);
    
    SND_Sample(const SND_Sample& sample) {
        this->chunk = sample.chunk;
        this->chunk_source = sample.chunk_source;
        this->millis = sample.millis;
        
        this->volume = sample.volume;
        this->frequency = sample.frequency;
        this->pan = sample.pan;
        this->external_looped_restart = sample.external_looped_restart;
        this->looped = sample.looped;
        this->steal_channel = sample.steal_channel;
        this->channel_group = sample.channel_group;
        this->global_volume_select = sample.global_volume_select;
    }

    ~SND_Sample();

    ///Loads raw data into sample and creates new chunk to hold it, assumes data is in device format
    bool loadRawData(uint8_t* src_data, size_t src_len, bool copy, const std::string& file_name);

    ///Plays provided sample as effect and returns used channel if any
    SND_Channel play();
    
    ///Updates the channel effects from current sample effects
    bool updateEffects();

    ///Returns channel if playing or -1 if none
    SND_Channel getChannel() const;

    ///Pause sample if its playing
    bool pause() const;

    ///resume sample if its paused and previously playing
    bool resume() const;

    ///Stops sample if its playing
    bool stop() const;

    ///Returns true if a channel is playing this sound
    bool isPlaying() const;

    ///Returns true if a channel was playing this sound and is paused
    bool isPaused() const;

#ifndef PERIMETER_SDL3
    ///Get the source chunk referenced by this sample
    Mix_Chunk* getChunkSource() {
        return chunk_source ? chunk_source.get()->chunk : nullptr;
    }
#endif
    
    ///Get the chunk referenced by this sample 
    SND_Chunk* getChunk() {
        return chunk ? chunk.get()->chunk : nullptr;
    }

    ///Get the sample duration in milliseconds, with frequency mod applied
    Uint64 getDuration() {
        return static_cast<Uint64>(static_cast<double>(millis) * this->frequency);
    }

private:
    ///Chunk pointer that is the source for the current one
    std::shared_ptr<MixChunkWrapper> chunk_source = nullptr;
    ///Current chunk to use
    std::shared_ptr<MixChunkWrapper> chunk = nullptr;
    
    //Duration of chunk in ms
    Uint64 millis = 0;

    ///Updates the channel effects from current sample effects, internal function that accepts channel
    bool updateEffects(SND_Channel channel);

#ifdef PERIMETER_SDL3
    //The current track playing this
    MIX_Track* track = nullptr;

#else
    ///Current frequency of chunk loaded
    ///(which might have been converted from chunk_source)
    float chunk_frequency = 1.0f;

    ///Returns true if current chunk needs frequency change
    bool needFrequencyChange() const;

    ///Convert source chunk data into desired frequency and stores in chunk
    bool convertChunkFrequency();
#endif
};

#endif //PERIMETER_SAMPLE_H
