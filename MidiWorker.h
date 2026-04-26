#pragma once
#include <QThread>
#include <RtMidi.h>
#include "MidiMap.h"

class MidiWorker : public QThread
{
    Q_OBJECT

public:
    explicit MidiWorker(QObject *parent = nullptr);
    ~MidiWorker();
    void stop();

    void setMidiMap(MidiMap *map) { m_midiMap = map; }

public slots:
    void sendEncoderNRPN(int encoderId, int value);
    void sendButtonNote(int buttonId, bool pressed);
    Q_INVOKABLE void sendNoteOn(int note, int channel);
    Q_INVOKABLE void sendNoteOff(int note, int channel);
    Q_INVOKABLE void sendConfigSelect(int configIndex);

signals:
    void midiReceived(int encoderId, int value);

protected:
    void run() override;

private:
    RtMidiOut *m_midiOut  = nullptr;
    RtMidiIn  *m_midiIn   = nullptr;
    bool       m_running  = false;
    MidiMap   *m_midiMap  = nullptr;

    // NRPN state machine
    struct NrpnState {
        int paramMSB = -1;
        int paramLSB = -1;
        int valMSB   = -1;
        int valLSB   = -1;
    } m_nrpn;

    void sendMessage(const std::vector<unsigned char> &msg);

    static void midiInCallback(double timestamp,
                                std::vector<unsigned char> *message,
                                void *userData);
    void processCC(uint8_t cc, uint8_t value);
};
