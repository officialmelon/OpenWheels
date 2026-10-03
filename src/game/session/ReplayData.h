#pragma once

// ReplayData: per-frame control bytes of one run, at most 18000 frames. Bits as passed to
// CharacterB2D::setState: 0x01 forward, 0x02 back, 0x04 lean forward, 0x08 lean back,
// 0x10 special/grab, 0x80 eject. Plain class (no vtable), sizeof 0x4660 (arm64).

class ReplayData
{
public:
    ReplayData();
    ~ReplayData();

    bool getCompleted();
    void setCompleted(bool completed);
    int getLength();
    void setLength(int length);  // empty in 1.1.3
    void addEntry(unsigned char entry);
    unsigned char getEntry();  // next entry, 0 at the end
    void resetPosition();
    void reset();
    unsigned char* getData();
    void setData(unsigned char* data);  // empty in 1.1.3
    bool replayComplete();

protected:
    // Names from the iOS original's ReplayData ivars.
    long _byteIndex;                    // +0x0000 index of the last entry written (-1 when empty)
    int _replayIndex;                   // +0x0008 index of the last entry read (-1)
    unsigned char _byteArray[18000];    // +0x000c addEntry stops at _byteIndex == 18000, so the
                                        //         18001st entry lands on _completed (sic)
    bool _completed;                    // +0x465c (cleared together with _byteArray by reset())
};
