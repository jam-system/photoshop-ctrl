#include "MidiWorker.h"
#include <cstdio>

MidiWorker::MidiWorker(QObject *parent)
    : QThread(parent)
{}

MidiWorker::~MidiWorker()
{
    stop();
    delete m_midiOut;
    delete m_midiIn;
}

void MidiWorker::stop()
{
    m_running = false;
    wait();
}

void MidiWorker::sendMessage(const std::vector<unsigned char> &msg)
{
    if (m_midiOut && m_midiOut->isPortOpen())
        m_midiOut->sendMessage(&msg);
}

// ── Sending ───────────────────────────────────────────────────────────────

void MidiWorker::sendEncoderNRPN(int encoderId, int value)
{
    if (!m_midiMap) return;

    int cc      = m_midiMap->encCC[encoderId];
    int channel = m_midiMap->currentChannel(); // bank A or B

    std::vector<unsigned char> msg;

    // NRPN param MSB (CC 99)
    msg = {(unsigned char)(0xB0 | channel),
           99, (unsigned char)((cc >> 7) & 0x7F)};
    sendMessage(msg);

    // NRPN param LSB (CC 98)
    msg = {(unsigned char)(0xB0 | channel),
           98, (unsigned char)(cc & 0x7F)};
    sendMessage(msg);

    // Data entry MSB (CC 6)
    msg = {(unsigned char)(0xB0 | channel),
           6, (unsigned char)((value >> 7) & 0x7F)};
    sendMessage(msg);

    // Data entry LSB (CC 38)
    msg = {(unsigned char)(0xB0 | channel),
           38, (unsigned char)(value & 0x7F)};
    sendMessage(msg);
}

void MidiWorker::sendButtonNote(int buttonId, bool pressed)
{
    if (!m_midiMap) return;

    // Encoder button notes use current bank channel
    int note    = m_midiMap->encNote[buttonId];
    int channel = m_midiMap->currentChannel();

    std::vector<unsigned char> msg = {
        (unsigned char)((pressed ? 0x90 : 0x80) | channel),
        (unsigned char)note,
        (unsigned char)(pressed ? 127 : 0)
    };
    sendMessage(msg);
}

void MidiWorker::sendNoteOn(int note, int channel)
{
    // channel is already 0-indexed
    std::vector<unsigned char> msg = {
        (unsigned char)(0x90 | channel),
        (unsigned char)note,
        127
    };
    sendMessage(msg);
}

void MidiWorker::sendNoteOff(int note, int channel)
{
    std::vector<unsigned char> msg = {
        (unsigned char)(0x80 | channel),
        (unsigned char)note,
        0
    };
    sendMessage(msg);
}

void MidiWorker::sendConfigSelect(int configIndex)
{
    if (!m_midiMap) return;

    int note    = m_midiMap->configSelectStartNote + configIndex;
    int channel = m_midiMap->configSelectChannel;

    // Note On then Off
    sendNoteOn(note, channel);
    QThread::msleep(50);
    sendNoteOff(note, channel);
}

// ── Receiving ─────────────────────────────────────────────────────────────

void MidiWorker::midiInCallback(double /*timestamp*/,
                                  std::vector<unsigned char> *message,
                                  void *userData)
{
    if (!message || message->size() < 3) return;
    MidiWorker *self = static_cast<MidiWorker*>(userData);

    uint8_t status  = (*message)[0] & 0xF0;
    uint8_t cc      = (*message)[1];
    uint8_t value   = (*message)[2];

    if (status == 0xB0)
        self->processCC(cc, value);
}

void MidiWorker::processCC(uint8_t cc, uint8_t value)
{
    switch (cc) {
        case 99: m_nrpn.paramMSB = value; break;
        case 98: m_nrpn.paramLSB = value; break;
        case 6:  m_nrpn.valMSB   = value; break;
        case 38:
            m_nrpn.valLSB = value;
            if (m_nrpn.paramMSB >= 0 && m_nrpn.paramLSB >= 0 &&
                m_nrpn.valMSB   >= 0 && m_midiMap) {

                int param = (m_nrpn.paramMSB << 7) | m_nrpn.paramLSB;
                int val   = (m_nrpn.valMSB   << 7) | m_nrpn.valLSB;

                // Find which encoder this CC belongs to
                for (int i = 0; i < 8; ++i) {
                    if (m_midiMap->encCC[i] == param) {
                        emit midiReceived(i, val);
                        break;
                    }
                }
                m_nrpn = NrpnState{};
            }
            break;
    }
}

// ── Thread ────────────────────────────────────────────────────────────────

void MidiWorker::run()
{
    try {
        m_midiOut = new RtMidiOut();
        m_midiIn  = new RtMidiIn();

        int outPort = -1, inPort = -1;

        for (unsigned int i = 0; i < m_midiOut->getPortCount(); ++i) {
            std::string name = m_midiOut->getPortName(i);
            printf("MIDI out port %d: %s\n", i, name.c_str());
            if (name.find("f_midi") != std::string::npos)
                outPort = i;
        }

        for (unsigned int i = 0; i < m_midiIn->getPortCount(); ++i) {
            std::string name = m_midiIn->getPortName(i);
            printf("MIDI in port %d: %s\n", i, name.c_str());
            if (name.find("f_midi") != std::string::npos)
                inPort = i;
        }

        if (outPort >= 0) {
            m_midiOut->openPort(outPort);
            printf("MidiWorker: opened out port %d\n", outPort);
        } else {
            printf("MidiWorker: no MIDI out port found\n");
        }

        if (inPort >= 0) {
            m_midiIn->openPort(inPort);
            m_midiIn->setCallback(&MidiWorker::midiInCallback, this);
            m_midiIn->ignoreTypes(false, false, false);
            printf("MidiWorker: opened in port %d\n", inPort);
        }

    } catch (RtMidiError &e) {
        printf("MidiWorker error: %s\n", e.getMessage().c_str());
        return;
    }

    m_running = true;
    while (m_running)
        QThread::msleep(100);
}
