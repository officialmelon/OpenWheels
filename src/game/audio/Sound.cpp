#include "Sound.h"

#include "SoundController.h"
#include "qol/QoL.h"  // QOL (PC addition)

#include "audio/include/AudioEngine.h"
#include "Box2D/Box2D.h"

#include <cmath>

using cocos2d::experimental::AudioEngine;

// @00613b48
int Sound::playSound(std::string name, float volume, float pitch, float pan, bool loop)
{
    std::string path = getFullPath(name, "ogg");
    // QOL (PC addition): the effects volume slider (1 by default).
    return AudioEngine::play2d(path, loop, SoundController::getMasterVolume() * volume * qol::effectsVolume());
}

// @00613c70
std::string Sound::getFullPath(std::string name, std::string extension)
{
    return name + "." + extension;
}

// @00613df0
void Sound::stopSound(int audioId)
{
    AudioEngine::stop(audioId);
}

// @00613df4
bool Sound::init(std::string fileName, b2Body* body, cocos2d::Vec2 position,
                 cocos2d::Vec2 listenerPosition, float maxVolume, float masterVolume,
                 bool playImmediately, bool loop, float defaultPitch, float masterPitch)
{
    if (fileName.empty())
    {
        return false;
    }

    _finishedPlaying = false;
    _finishCallback = nullptr;
    _soundId = -1;
    _fileName = fileName;
    _maxVolume = maxVolume;
    _defaultPitch = defaultPitch;
    if (body)
    {
        const b2Vec2& bodyPosition = body->GetPosition();
        position = cocos2d::Vec2(bodyPosition.x, bodyPosition.y);
    }
    _body = body;
    _soundPosition = position;
    _looping = loop;

    if (playImmediately)
    {
        if (!play(1.0f, false))
        {
            return false;
        }
    }
    else
    {
        AudioEngine::preload(getFullPath(fileName, "ogg"));
    }

    updatePosition(listenerPosition.x, listenerPosition.y, masterVolume);
    return true;
}

// @006140a8
bool Sound::play(float volume, bool loop)
{
    if (_soundId == -1)
    {
        std::string path = getFullPath(_fileName, "ogg");
        int soundId = AudioEngine::play2d(path, loop, volume * qol::effectsVolume());  // QOL (PC addition)
        _soundId = soundId;
        if (soundId != -1)
        {
            if (_looping)
            {
                AudioEngine::setLoop(soundId, true);
            }
            AudioEngine::setFinishCallback(_soundId, [this](int, const std::string&) {
                soundFinishedPlaying();
            });
        }
        if (soundId == -1)
        {
            return false;
        }
    }
    return true;
}

// @006142ac
bool Sound::updatePosition(float listenerX, float listenerY, float masterVolume)
{
    if (_soundId == -1)
    {
        return true;
    }
    if (_finishedPlaying)
    {
        return false;
    }

    if (_steps != 0)
    {
        updateFade();
    }

    if (_body)
    {
        const b2Vec2& bodyPosition = _body->GetPosition();
        _soundPosition = cocos2d::Vec2(bodyPosition.x, bodyPosition.y);
    }

    float dx = fabsf(_soundPosition.x - listenerX);
    float dy = fabsf(_soundPosition.y - listenerY);
    if (dx > 16.0f || dy > 8.0f)
    {
        AudioEngine::setVolume(_soundId, 0.0f);
        return _looping;
    }

    float volumeX = (1.0f - dx / 16.0f) * _maxVolume;
    float volumeY = (1.0f - dy / 8.0f) * _maxVolume;
    // QOL (PC addition): the effects volume slider (1 by default).
    AudioEngine::setVolume(_soundId, fminf(volumeX, volumeY) * masterVolume * qol::effectsVolume());
    return true;
}

// @006143dc
void Sound::updateFade()
{
    if (--_steps != 0)
    {
        _maxVolume = _volumeIncrement + _maxVolume;
        return;
    }
    _maxVolume = _targetVolume;
    if (_stopOnComplete)
    {
        stop();
    }
}

// @00614420
void Sound::stop()
{
    _finishedPlaying = true;
    if (_soundId != -1)
    {
        AudioEngine::stop(_soundId);
    }
}

// @00614440
void Sound::pause()
{
    if (_soundId != -1)
    {
        AudioEngine::pause(_soundId);
    }
}

// @00614454
void Sound::setMasterPitch(float masterPitch)
{
    // AudioEngine (cocos2d-x 3.17) has no pitch control.
}

// @00614458
void Sound::setDefaultPitch(float defaultPitch)
{
    _defaultPitch = defaultPitch;
}

// @00614460
float Sound::getDefaultPitch()
{
    return _defaultPitch;
}

// @00614468
void Sound::setFinishCallback(const std::function<void(int&)>& callback)
{
    _finishCallback = callback;
}

// @0061453c
void Sound::soundFinishedPlaying()
{
    AudioEngine::stop(_soundId);
    _finishedPlaying = true;
}

// @00614568
void Sound::triggerCallback()
{
    if (_finishCallback)
    {
        _finishCallback(_soundId);
    }
}

// @00614588
bool Sound::isPlaying()
{
    return _soundId != -1 && !_finishedPlaying;
}

// @006145a0
void Sound::setMaxVolume(float maxVolume)
{
    _maxVolume = maxVolume;
}

// @006145a8
b2Body* Sound::getBody()
{
    return _body;
}

// @006145b0
void Sound::setBody(b2Body* body)
{
    _body = body;
}

// @006145b8
void Sound::fadeTo(float targetVolume, float duration, bool stopOnComplete)
{
    if (_targetVolume == targetVolume && _stopOnComplete == stopOnComplete)
    {
        return;
    }
    if (_maxVolume == targetVolume)
    {
        if (stopOnComplete)
        {
            stop();
        }
        return;
    }
    _targetVolume = targetVolume;
    _stopOnComplete = stopOnComplete;
    _steps = roundf(duration / (1.0f / 60.0f));
    _volumeIncrement = (targetVolume - _maxVolume) / _steps;
}
