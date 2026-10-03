#pragma once

#include "math/Vec2.h"

#include <functional>
#include <string>

class b2Body;

// One positional sound played through cocos2d::experimental::AudioEngine (arm64 sizeof 0x90).
// Created by SoundController::createBodySound / createPositionSound (new (std::nothrow) Sound, then
// init) and owned by the SoundController, which deletes it once updatePosition() returns false.
//
// Polymorphic but with NO virtual destructor (the vtable has exactly the 7 slots below, in this order;
// the owner deletes it as a plain Sound with the destructor inlined). Constructor and destructor are
// inline: there are no out-of-line symbols for either.
//
// File names are base names; getFullPath(name, "ogg") appends ".ogg" and the "sounds" search path
// resolves it. Volume attenuation (updatePosition): silent beyond 16 m horizontally or 8 m vertically
// from the listener, otherwise maxVolume * min(1 - dx/16, 1 - dy/8) * masterVolume.
// Pitch is not supported by AudioEngine 3.17, so setMasterPitch is empty and the pitch arguments are
// only stored.
//
// The iOS ancestors are FFAreaSound (_soundPosition, _maxVolume, _defaultPitch, _looping, _steps,
// _targetVolume, _volumeIncrement, _stopOnComplete) and HWDynamicAreaSound (_body).
class Sound
{
public:
    Sound()
        : _soundPosition()
        , _soundId(-1)
        , _fileName()
        , _maxVolume(0.0f)
        , _targetVolume(0.0f)
        , _steps(0)
        , _volumeIncrement(0.0f)
        // _stopOnComplete (+0x40) is left uninitialised by the original constructor
        , _defaultPitch(0.0f)
        , _looping(false)
        , _finishedPlaying(false)
        , _body(nullptr)
        , _finishCallback()
    {
    }

    // Fire-and-forget effect: AudioEngine::play2d(getFullPath(name, "ogg"), loop,
    // volume * SoundController::getMasterVolume()). pitch and pan are ignored. Returns the audio id.
    static int playSound(std::string name, float volume, float pitch, float pan, bool loop); // @00613b48
    // name + "." + extension
    static std::string getFullPath(std::string name, std::string extension);  // @00613c70
    // AudioEngine::stop(audioId)
    static void stopSound(int audioId);                                        // @00613df0

    // Returns false for an empty fileName or when playImmediately and play() fails. body (if any)
    // overrides position. playImmediately == false only preloads the file. Finishes with
    // updatePosition(listenerPosition, masterVolume). masterPitch is not used.
    bool init(std::string fileName, b2Body* body, cocos2d::Vec2 position,
              cocos2d::Vec2 listenerPosition, float maxVolume, float masterVolume,
              bool playImmediately, bool loop, float defaultPitch, float masterPitch); // @00613df4

    // ---- virtuals, in vtable order --------------------------------------------------------------
    virtual void setMasterPitch(float masterPitch);                            // @00614454 vptr+0x00 (empty)
    virtual void setDefaultPitch(float defaultPitch);                          // @00614458 vptr+0x08
    virtual float getDefaultPitch();                                           // @00614460 vptr+0x10
    // Steps the volume fade, follows the body, applies distance attenuation. Returns false when the
    // sound is finished (or out of range and not looping) so the owner can delete it.
    virtual bool updatePosition(float listenerX, float listenerY, float masterVolume); // @006142ac vptr+0x18
    // Starts playback once (no-op returning true when already started); returns false if play2d fails.
    virtual bool play(float volume, bool loop);                                // @006140a8 vptr+0x20
    virtual void pause();                                                      // @00614440 vptr+0x28
    virtual void stop();                                                       // @00614420 vptr+0x30

    void updateFade();                                                         // @006143dc
    void setFinishCallback(const std::function<void(int&)>& callback);         // @00614468
    // AudioEngine::stop(_soundId); _finishedPlaying = true. (Also the AudioEngine finish callback.)
    void soundFinishedPlaying();                                               // @0061453c
    // _finishCallback(_soundId) if set.
    void triggerCallback();                                                    // @00614568
    bool isPlaying();                                                          // @00614588
    void setMaxVolume(float maxVolume);                                        // @006145a0
    b2Body* getBody();                                                         // @006145a8
    void setBody(b2Body* body);                                                // @006145b0
    // Fades to targetVolume over duration seconds in 1/60 s steps; optionally stop() at the end.
    void fadeTo(float targetVolume, float duration, bool stopOnComplete);      // @006145b8

private:
    // vptr                                                                    // +0x00
    cocos2d::Vec2 _soundPosition;                    // +0x08 metres; copied from the body each update
    int _soundId;                                    // +0x10 AudioEngine audio id, -1 = not started
    std::string _fileName;                           // +0x18 base name (no extension)
    float _maxVolume;                                // +0x30
    float _targetVolume;                             // +0x34 fade target
    unsigned int _steps;                             // +0x38 fade frames left (fcvtau/ucvtf: unsigned)
    float _volumeIncrement;                          // +0x3c per-frame fade step
    bool _stopOnComplete;                            // +0x40 stop() when the fade ends
    float _defaultPitch;                             // +0x44
    bool _looping;                                   // +0x48
    bool _finishedPlaying;                           // +0x49
    b2Body* _body;                                   // +0x50
    std::function<void(int&)> _finishCallback;       // +0x60 (16-byte aligned)
};
