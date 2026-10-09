#include "SoundController.h"

#include "Sound.h"
#include "SoundList.h"
#include "qol/QoL.h"  // QOL (PC addition)
#include "input/Haptics.h"  // PAD (PC addition)
#include "Box2D/Box2D.h"  // PAD (PC addition): body positions for the rumble's distance

#include "audio/include/AudioEngine.h"
#include "cocos2d.h"

#include <cmath>

USING_NS_CC;
using cocos2d::experimental::AudioEngine;

float SoundController::_themeMusicSoundLimiter = 0.7f;  // @00abb644
float SoundController::_masterVolume;                   // @00ac65a4
int SoundController::_musicId;                          // @00ac65a0

// @0061483c
SoundController::SoundController()
    : _sounds()
    , _position()
    , _paused(false)
{
    _masterVolume = 0.0f;
    _musicId = -1;
    init();
}

// @006148f8
bool SoundController::init()
{
    _areaSoundPitch = 1.0f;
    int volumeOffset = UserDefault::getInstance()->getIntegerForKey("sound_volume_offset");
    _paused = false;
    _masterVolume = (volumeOffset + 100) / 100.0f;
    _soundList = new SoundList();
    return true;
}

// @0061497c
SoundController::~SoundController()
{
    CC_SAFE_DELETE(_soundList);
}

// @006149cc
std::string SoundController::soundFileName(int soundId)
{
    return _soundList->soundFileNameForId(soundId);
}

// @006149d4
int SoundController::playSound(std::string name, float volume, float pitch, float pan)
{
    openwheels::haptics::onSound(name, 0.0f);  // PAD (PC addition): controller rumble
    return Sound::playSound(name, 1.0f, 1.0f, 0.0f, false);
}

// @00614a7c
void SoundController::preloadSound(std::string name)
{
    AudioEngine::preload(Sound::getFullPath(name, "ogg"));
}

// @00614be8
int SoundController::playBackgroundMusic(std::string name)
{
    if (_masterVolume == 0.0f || _musicId != -1)
    {
        return -1;
    }
    if (name.empty())
    {
        name = "SuperPretzel";
    }
    _musicId = AudioEngine::play2d(name + ".mp3", false, 1.0f);
    AudioEngine::setVolume(_musicId, _masterVolume * _themeMusicSoundLimiter * qol::musicVolume());  // QOL (PC addition): music slider
    return _musicId;
}

// @00614dc0
void SoundController::stopBackgroundMusic()
{
    // _musicId is not reset, so playBackgroundMusic() refuses to start music again afterwards.
    if (_musicId != -1)
    {
        AudioEngine::stop(_musicId);
    }
}

// @00614ddc
bool SoundController::isSoundPlaying(int audioId)
{
    return AudioEngine::getState(audioId) == AudioEngine::AudioState::PLAYING;
}

// @00614dfc
bool SoundController::stopSoundsForBody(b2Body* body)
{
    for (auto it = _sounds.begin(); it != _sounds.end(); ++it)
    {
        Sound* sound = *it;
        if (sound->getBody() == body)
        {
            sound->soundFinishedPlaying();
        }
    }
    return false;
}

// @00614e68
Sound* SoundController::createBodySound(std::string name, b2Body* body, float pitch, bool loop)
{
    // PAD (PC addition): controller rumble for one-shot sounds, faded by the distance.
    if (!loop && body)
    {
        const b2Vec2 p = body->GetPosition();
        openwheels::haptics::onSound(name, _position.distance(Vec2(p.x, p.y)));
    }
    if (!canPlayAnotherSound())
    {
        return nullptr;
    }
    Sound* sound = new (std::nothrow) Sound();
    if (sound->init(name, body, Vec2(), _position, 1.0f, _masterVolume, true, loop, pitch, _areaSoundPitch)
        && addSound(sound))
    {
        return sound;
    }
    delete sound;
    return nullptr;
}

// @0061511c
bool SoundController::canPlayAnotherSound()
{
    return _sounds.size() < 24;
}

// @00615130
bool SoundController::addSound(Sound* sound)
{
    if (sound && canPlayAnotherSound())
    {
        _sounds.push_back(sound);
        return true;
    }
    return false;
}

// @00615270
Sound* SoundController::createPositionSound(std::string name, Vec2 position, float pitch, bool loop)
{
    if (!loop) openwheels::haptics::onSound(name, _position.distance(position));  // PAD (PC addition)
    if (!canPlayAnotherSound())
    {
        return nullptr;
    }
    Sound* sound = new (std::nothrow) Sound();
    if (sound->init(name, nullptr, position, _position, 1.0f, _masterVolume, true, loop, pitch,
                    _areaSoundPitch)
        && addSound(sound))
    {
        return sound;
    }
    delete sound;
    return nullptr;
}

// @00615530
void SoundController::update(Vec2 listenerPosition)
{
    if (_paused)
    {
        return;
    }
    _position = listenerPosition;
    auto it = _sounds.begin();
    while (it != _sounds.end())
    {
        Sound* sound = *it;
        if (!sound->updatePosition(listenerPosition.x, listenerPosition.y, _masterVolume))
        {
            sound->stop();
            sound->triggerCallback();
            it = _sounds.erase(it);
            delete sound;
        }
        else
        {
            ++it;
        }
    }
}

// @0061565c
void SoundController::setPosition(Vec2 position)
{
    _position = position;
}

// @00615668
void SoundController::setAreaSoundPitch(float pitch)
{
    _areaSoundPitch = pitch;
    for (unsigned int i = 0; i < _sounds.size(); i++)
    {
        _sounds[i]->setMasterPitch(pitch);
    }
}

// @006156cc
void SoundController::setMasterVolume(float volume)
{
    _masterVolume = fmaxf(fminf(volume, 1.0f), 0.0f);
    if (_musicId != -1)
    {
        AudioEngine::setVolume(_musicId, _masterVolume * _themeMusicSoundLimiter * qol::musicVolume());  // QOL (PC addition): music slider
    }
}

// @00615714
float SoundController::getMasterVolume()
{
    return _masterVolume;
}

// @00615724
float SoundController::areaSoundPitch()
{
    return _areaSoundPitch;
}

// @0061572c
void SoundController::setPaused(bool paused)
{
    if (paused)
    {
        AudioEngine::pauseAll();
    }
    else
    {
        AudioEngine::resumeAll();
    }
    _paused = paused;
}

// @00615764
void SoundController::stopAllSounds(bool removeSounds)
{
    for (unsigned int i = 0; i < _sounds.size(); i++)
    {
        _sounds[i]->stop();
    }
    if (removeSounds)
    {
        auto it = _sounds.begin();
        while (it != _sounds.end())
        {
            Sound* sound = *it;
            sound->stop();
            sound->triggerCallback();
            it = _sounds.erase(it);
            delete sound;
        }
        _sounds.clear();
    }
}

// @00615898
void SoundController::reset()
{
    for (unsigned int i = 0; i < _sounds.size(); i++)
    {
        _sounds[i]->stop();
    }
    _paused = false;
}
