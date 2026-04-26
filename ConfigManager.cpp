#include "ConfigManager.h"
#include <QNetworkReply>
#include <QSettings>
#include <QTemporaryFile>
#include <QDebug>

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
{}

void ConfigManager::loadFromServer(const QString &baseUrl)
{
    m_baseUrl = baseUrl;
    fetchMidiMap();
}

// ── MIDI map ──────────────────────────────────────────────────────────────

void ConfigManager::fetchMidiMap()
{
    QNetworkReply *reply = m_net.get(
        QNetworkRequest(QUrl(m_baseUrl + "/midi_map.ini")));

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "ConfigManager: failed to fetch midi_map.ini:"
                       << reply->errorString();
            qWarning() << "ConfigManager: using default MIDI map";
        } else {
            parseMidiMap(reply->readAll());
        }
        reply->deleteLater();
        emit midiMapLoaded();
        fetchIndex();
    });
}

void ConfigManager::parseMidiMap(const QByteArray &data)
{
    // Write to temp file for QSettings INI parser
    QTemporaryFile tmp;
    tmp.setAutoRemove(true);
    if (!tmp.open()) {
        qWarning() << "ConfigManager: cannot create temp file for midi_map";
        return;
    }
    tmp.write(data);
    tmp.flush();

    QSettings s(tmp.fileName(), QSettings::IniFormat);

    // Channels — INI stores 1-indexed, RtMidi uses 0-indexed
    m_midiMap.channelA    = s.value("midi/channel_a",    11).toInt() - 1;
    m_midiMap.channelB    = s.value("midi/channel_b",    10).toInt() - 1;
    m_midiMap.channelCtrl = s.value("midi/channel_ctrl", 12).toInt() - 1;

    // Encoder CCs and notes
    for (int i = 0; i < 8; ++i) {
        m_midiMap.encCC[i]   = s.value(
            QString("encoders/enc_%1_cc").arg(i),   211 + i).toInt();
        m_midiMap.encNote[i] = s.value(
            QString("encoders/enc_%1_note").arg(i),  24 + i).toInt();
    }

    // Grid buttons
    m_midiMap.gridStartNote = s.value("buttons/grid_start_note", 32).toInt();
    m_midiMap.gridChannel   = s.value("buttons/grid_channel",    11).toInt() - 1;

    // Config select
    m_midiMap.configSelectChannel   =
        s.value("config_select/channel",    12).toInt() - 1;
    m_midiMap.configSelectStartNote =
        s.value("config_select/start_note", 10).toInt();

    // Control buttons
    m_midiMap.toggleBankRow = s.value("control_buttons/toggle_bank_row", 5).toInt();
    m_midiMap.toggleBankCol = s.value("control_buttons/toggle_bank_col", 4).toInt();

    qDebug() << "ConfigManager: MIDI map loaded"
             << "chA=" << m_midiMap.channelA + 1
             << "chB=" << m_midiMap.channelB + 1
             << "chCtrl=" << m_midiMap.channelCtrl + 1;
}

// ── Config loading ────────────────────────────────────────────────────────

void ConfigManager::fetchIndex()
{
    QNetworkReply *reply = m_net.get(
        QNetworkRequest(QUrl(m_baseUrl + "/index.json")));

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "ConfigManager: failed to fetch index:"
                       << reply->errorString();
            reply->deleteLater();
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonArray arr    = doc.object()["configs"].toArray();

        m_configNames.clear();
        for (const auto &v : arr)
            m_configNames << v.toString();

        m_configs.resize(m_configNames.size());
        m_loadedCount = 0;

        qDebug() << "ConfigManager: found" << m_configNames.size() << "configs";

        for (int i = 0; i < m_configNames.size(); ++i)
            fetchConfig(m_configNames[i], i);

        reply->deleteLater();
    });
}

void ConfigManager::fetchConfig(const QString &filename, int idx)
{
    QNetworkReply *reply = m_net.get(
        QNetworkRequest(QUrl(m_baseUrl + "/" + filename)));

    connect(reply, &QNetworkReply::finished, this, [this, reply, idx]() {
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "ConfigManager: failed to fetch config:"
                       << reply->errorString();
            reply->deleteLater();
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        m_configs[idx]    = doc.object().toVariantMap();
        m_loadedCount++;

        qDebug() << "ConfigManager: loaded config" << idx
                 << doc.object()["name"].toString();

        if (m_loadedCount == m_configNames.size()) {
            emit configsChanged();
            emit activeConfigChanged();
            emit allLoaded();
            qDebug() << "ConfigManager: all configs loaded";
        }

        reply->deleteLater();
    });
}

// ── Actions ───────────────────────────────────────────────────────────────

void ConfigManager::selectConfig(int index)
{
    if (index < 0 || index >= m_configs.size()) return;
    m_activeIndex = index;
    emit activeConfigChanged();
}

void ConfigManager::toggleBank()
{
    m_midiMap.activeBank = (m_midiMap.activeBank == 0) ? 1 : 0;
    emit activeBankChanged();
    qDebug() << "ConfigManager: bank switched to"
             << (m_midiMap.activeBank == 0 ? "A" : "B");
}

// ── Accessors ─────────────────────────────────────────────────────────────

QVariantList ConfigManager::buttons() const
{
    return encodersForKey("buttons");
}

QVariantList ConfigManager::encodersA() const
{
    return encodersForKey("encoders_a");
}

QVariantList ConfigManager::encodersB() const
{
    return encodersForKey("encoders_b");
}

QVariantList ConfigManager::encodersForKey(const QString &key) const
{
    if (m_configs.isEmpty() || m_activeIndex >= m_configs.size())
        return {};
    return m_configs[m_activeIndex].toMap()[key].toList();
}
