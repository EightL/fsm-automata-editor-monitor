#ifndef UI_QT_RUNTIME_CLIENT_HPP
#define UI_QT_RUNTIME_CLIENT_HPP

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QString>
#include <QMap>
#include <QProcess>
#include <memory>
#include "channel.hpp"
#include <nlohmann/json.hpp>
#include "udp_channel.hpp"

// A lightweight snapshot of one “state” message
struct StateSnapshot {
    quint64      seq;
    qint64       ts;
    QString      state;
    QMap<QString, QString> inputs;
    QMap<QString, QString> vars;
    QMap<QString, QString> outputs;
};
Q_DECLARE_METATYPE(StateSnapshot)

class RuntimeClient : public QObject {
    Q_OBJECT
public:
    /// bindAddr e.g. "0.0.0.0:45455", peerAddr e.g. "127.0.0.1:45454"
    explicit RuntimeClient(QString bindAddr,
                           QString peerAddr,
                           QObject* parent = nullptr);

    ~RuntimeClient() override;

    /// Call once (from GUI thread) to kick off the polling thread/loop.
    void start();
    void stop();
    void shutdown();
    void setVariable(QString name, QString value);

    void sendCustomMessage(const std::string& jsonMessage);

signals:
    /// Emitted whenever we receive a “state” JSON over UDP
    void stateReceived(StateSnapshot snapshot);
    void logMessage(const QString& message); // New signal for logging

private slots:
    /// Runs in the worker thread: sets up the timer
    void onThreadStarted();

    /// Polls socket, emits stateReceived() for each packet
    void pollChannel();

private:
    const QString            m_bindAddr;
    const QString            m_peerAddr;
    std::shared_ptr<io_bridge::IChannel> m_channel;

    QThread*                 m_thread{nullptr};
    QTimer*                  m_timer{nullptr};
};

#endif // UI_QT_RUNTIME_CLIENT_HPP
