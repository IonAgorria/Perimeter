//
// Created by Ion Agorria on 9/06/24
//
#ifndef PERIMETER_SAMPLEPARAMS_H
#define PERIMETER_SAMPLEPARAMS_H

//Mixer channel groups
enum SND_GROUP {
    SND_GROUP_SPEECH = 0,
    SND_GROUP_EFFECTS,
    SND_GROUP_EFFECTS_ONCE,
    SND_GROUP_EFFECTS_LOOPED,
#ifdef PERIMETER_SDL3
    SND_GROUP_MUSIC,
#endif
    //To count number of enums
    SND_GROUPS_COUNT
};

#define SND_NO_CHANNEL_GROUP (-1)
#define SND_NO_CHANNEL_INDEX (-1)

#ifdef PERIMETER_SDL3
#define SND_NO_CHANNEL (nullptr)
#else
#define SND_NO_CHANNEL SND_NO_CHANNEL_INDEX
#endif

//Which volume to use
enum GLOBAL_VOLUME {
    GLOBAL_VOLUME_CHANNEL = 0, //Use voice or effects volume according to current channel
    GLOBAL_VOLUME_VOICE,
    GLOBAL_VOLUME_EFFECTS,
    GLOBAL_VOLUME_IGNORE,
};

#endif //PERIMETER_SAMPLEPARAMS_H
