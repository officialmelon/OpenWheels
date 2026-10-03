#include "ReplayData.h"

#include <cstring>

// @006091d0
ReplayData::ReplayData()
    : _byteIndex(-1)
    , _replayIndex(-1)
{
    // _byteArray and _completed are left uninitialised, as in the original (reset() clears them).
}

// @006091e4
ReplayData::~ReplayData()
{
}

// @006091e8
bool ReplayData::getCompleted()
{
    return _completed;
}

// @006091f4
void ReplayData::setCompleted(bool completed)
{
    _completed = completed;
}

// @00609200
int ReplayData::getLength()
{
    return (int)_byteIndex + 1;
}

// @0060920c
void ReplayData::setLength(int length)
{
}

// @00609210
void ReplayData::addEntry(unsigned char entry)
{
    if (_byteIndex != 18000)
    {
        _byteIndex++;
        // RE-NOTE(@00609210): faithful to the original: the guard compares the index of the last
        // entry written with 18000, so the 18001st entry is stored at _byteArray[18000], one past
        // the array, i.e. on _completed (+0x465c). Written through getData() (the raw buffer
        // pointer, as the original addresses it from +0xc) so the out-of-bounds store is explicit
        // pointer arithmetic on the object's storage rather than an array-subscript overflow.
        getData()[_byteIndex] = entry;
    }
}

// @00609234
unsigned char ReplayData::getEntry()
{
    if (_byteIndex == _replayIndex)
    {
        return 0;
    }
    _replayIndex++;
    return _byteArray[_replayIndex];
}

// @00609260
void ReplayData::resetPosition()
{
    _replayIndex = -1;
}

// @0060926c
void ReplayData::reset()
{
    // The original issues a single memset of 18001 bytes from +0xc: the entry buffer plus
    // _completed. _replayIndex is not reset here (resetPosition() does that).
    _byteIndex = -1;
    memset(_byteArray, 0, sizeof(_byteArray) + sizeof(_completed));
}

// @00609280
unsigned char* ReplayData::getData()
{
    return _byteArray;
}

// @00609288
void ReplayData::setData(unsigned char* data)
{
}

// @0060928c
bool ReplayData::replayComplete()
{
    return _byteIndex == _replayIndex;
}
