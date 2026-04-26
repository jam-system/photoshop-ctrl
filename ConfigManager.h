#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantList>
#include <QVariantMap>
#include "MidiMap.h"

class ConfigManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList configs      READ configs      NOTIFY configsChanged)
    Q_PROPERTY(int          activeConfig READ activeConfig NOTIFY activeConfigChanged)
    Q_PROPERTY(QVariantList buttons      READ buttons      NOTIFY activeConfigChanged)
    Q_PROPERTY(QVariantList encodersA    READ encodersA    NOTIFY activeConfigChanged)
    Q_PROPERTY(QVariantList encodersB    READ encodersB    NOTIFY activeConfigChanged)
    Q_PROPERTY(int          activeBank   READ activeBank   NOTIFY activeBankChanged)

public:
    explicit ConfigManager(QObject *parent = nullptr);

    void loadFromServer(const QString &baseUrl);

    QVariantList configs()      const { return m_configs; }
    int          activeConfig() const { return m_activeIndex; }
    QVariantList buttons()      const;
    QVariantList encodersA()    const;
    QVariantList encodersB()    const;
    int          activeBank()   const { return m_midiMap.activeBank; }

    // Expose midi map to QML for control button positions
    Q_INVOKABLE int toggleBankRow() const { return m_midiMap.toggleBankRow; }
    Q_INVOKABLE int toggleBankCol() const { return m_midiMap.toggleBankCol; }

    Q_INVOKABLE void selectConfig(int index);
    Q_INVOKABLE void toggleBank();

    MidiMap &midiMap() { return m_midiMap; }

signals:
    void configsChanged();
    void activeConfigChanged();
    void activeBankChanged();
    void allLoaded();
    void midiMapLoaded();

private:
    QNetworkAccessManager m_net;
    QString               m_baseUrl;
    QVariantList          m_configs;
    QStringList           m_configNames;
    int                   m_activeIndex  = 0;
    int                   m_loadedCount  = 0;
    bool                  m_midiMapReady = false;
    MidiMap               m_midiMap;

    void fetchMidiMap();
    void fetchIndex();
    void fetchConfig(const QString &filename, int idx);
    void parseMidiMap(const QByteArray &data);

    QVariantList encodersForKey(const QString &key) const;
};
