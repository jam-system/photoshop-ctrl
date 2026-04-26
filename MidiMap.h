#pragma once
#include <QObject>
#include <QVector>

// Holds the parsed midi_map.ini values.
// Shared between MidiWorker and EncoderWorker via pointer.
struct MidiMap {
    // Channels (0-indexed for RtMidi, so subtract 1 from INI values)
    int channelA    = 10;  // INI 11 → 10
    int channelB    =  9;  // INI 10 →  9
    int channelCtrl = 11;  // INI 12 → 11

    // Encoder CC numbers
    int encCC[8]   = {211,212,213,214,215,216,217,218};

    // Encoder button notes
    int encNote[8] = {24,25,26,27,28,29,30,31};

    // Grid buttons
    int gridStartNote = 32;
    int gridChannel   = 10;  // INI 11 → 10

    // Config select
    int configSelectChannel   = 11;  // INI 12 → 11
    int configSelectStartNote = 10;

    // Control buttons
    int toggleBankRow = 5;
    int toggleBankCol = 4;

    // Current bank (0 = A, 1 = B)
    int activeBank = 0;

    int currentChannel() const {
        return activeBank == 0 ? channelA : channelB;
    }
};
