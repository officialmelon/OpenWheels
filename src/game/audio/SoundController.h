#pragma once

#include "math/Vec2.h"

#include <string>
#include <vector>

class Sound;
class SoundList;
class b2Body;

// Game audio front end (no base class, no RTTI, arm64 sizeof 0x30; owned by Settings).
//
// Audio API: cocos2d::experimental::AudioEngine (cocos/audio/include/AudioEngine.h) for everything:
// play2d / setVolume / setLoop / setFinishCallback / stop / pause / pauseAll / resumeAll / getState /
// preload. SimpleAudioEngine is not used.
//   * one-shot effects: playSound() -> Sound::playSound(name) -> AudioEngine::play2d(name + ".ogg",
//     loop, volume * getMasterVolume()) - fire and forget, returns the AudioEngine id;
//   * positional/body sounds (createBodySound / createPositionSound) are Sound objects kept in _sounds
//     (at most 24, see canPlayAnotherSound); update(listener) attenuates them by distance every frame
//     and deletes the finished ones;
//   * music: playBackgroundMusic(name) plays name + ".mp3" ("SuperPretzel" when name is empty) at
//     _masterVolume * _themeMusicSoundLimiter (0.7), one track at a time (_musicId).
// Sound files are looked up through the FileUtils search paths ("sounds" is added by AppDelegate).
// The master volume comes from UserDefault "sound_volume_offset" (int, -100..0):
// (offset + 100) / 100.
//
// The iOS ancestor is FFSoundController (ivars _sounds, _position, _paused, _masterPitch,
// _masterVolume) + HWSoundController (_soundList).
class SoundController
{
public:
    SoundController();                                                         // @0061483c
    ~SoundController();                                                        // @0061497c
    // Same as the constructor's body (allocates another SoundList). Never called in 1.1.3.
    bool init();                                                               // @006148f8

    // _soundList->soundFileNameForId(soundId).
    std::string soundFileName(int soundId);                                    // @006149cc
    // Ignores volume/pitch/pan: always Sound::playSound(name, 1.0f, 1.0f, 0.0f, false). Every caller
    // passes (1, 1, 0).
    static int playSound(std::string name, float volume = 1.0f, float pitch = 1.0f,
                         float pan = 0.0f);                                    // @006149d4
    // AudioEngine::preload(name + ".ogg").
    void preloadSound(std::string name);                                       // @00614a7c
    // Returns the music's AudioEngine id, or -1 when muted (_masterVolume == 0) or music is already
    // playing.
    int playBackgroundMusic(std::string name);                                 // @00614be8
    void stopBackgroundMusic();                                                // @00614dc0
    // AudioEngine::getState(audioId) == AudioState::PLAYING.
    bool isSoundPlaying(int audioId);                                          // @00614ddc
    // Sound::soundFinishedPlaying() on every sound attached to body. Always returns false.
    bool stopSoundsForBody(b2Body* body);                                      // @00614dfc
    // New Sound following body (looping or not); nullptr when 24 sounds already exist or init fails.
    Sound* createBodySound(std::string name, b2Body* body, float pitch, bool loop); // @00614e68
    bool canPlayAnotherSound();                                                // @0061511c (_sounds.size() < 24)
    // Appends sound when there is room; returns whether it was added.
    bool addSound(Sound* sound);                                               // @00615130
    // New Sound at a fixed world position (metres).
    Sound* createPositionSound(std::string name, cocos2d::Vec2 position, float pitch,
                               bool loop);                                     // @00615270
    // Per-frame (from Session): stores the listener position and updates/reaps the sounds (no-op
    // while paused).
    void update(cocos2d::Vec2 listenerPosition);                               // @00615530
    void setPosition(cocos2d::Vec2 position);                                  // @0061565c
    // Stores the pitch and forwards it to every sound's setMasterPitch (a no-op on Android).
    void setAreaSoundPitch(float pitch);                                       // @00615668
    // Clamped to [0, 1]; also re-applies the music volume.
    static void setMasterVolume(float volume);                                 // @006156cc
    static float getMasterVolume();                                            // @00615714
    float areaSoundPitch();                                                    // @00615724
    // AudioEngine::pauseAll() / resumeAll().
    void setPaused(bool paused);                                               // @0061572c
    // stop() on every sound; with removeSounds also triggers their callbacks and deletes them.
    void stopAllSounds(bool removeSounds);                                     // @00615764
    // stop() on every sound and unpause.
    void reset();                                                              // @00615898

private:
    std::vector<Sound*> _sounds;            // +0x00 owned (deleted when finished)
    cocos2d::Vec2 _position;                // +0x18 listener position (metres), see update()
    bool _paused;                           // +0x20
    float _areaSoundPitch;                  // +0x24 iOS: _masterPitch (property areaSoundPitch)
    SoundList* _soundList;                  // +0x28 owned

    static float _themeMusicSoundLimiter;   // @00abb644 (.data) = 0.7f, music volume factor
    static float _masterVolume;             // @00ac65a4
    static int _musicId;                    // @00ac65a0 AudioEngine id of the music, -1 = none
};
