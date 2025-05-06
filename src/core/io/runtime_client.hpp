/**
 * @file   runtime_client.hpp
 * @brief  Declares RuntimeClient—a Qt QObject that manages a UDP channel
 *         to an external FSM interpreter—and the StateSnapshot struct.
 *
 * RuntimeClient wraps an io_bridge::UdpChannel to send control messages
 * (inject, setVar, shutdown) and to poll for “state” JSON packets, which
 * it emits as StateSnapshot signals on the Qt event loop.
 *
 * @author Martin Ševčík (xsevcim00)
 * @author Jakub Lůčný (xlucnyj00)
 * @date   2025-05-06
 */

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
#include "udp_channel.hpp"
#include <nlohmann/json.hpp>

/**
 * @struct StateSnapshot
 * @brief Lightweight snapshot of a “state” message received over UDP.
 *
 * Holds the sequence number, timestamp, current state identifier,
 * and maps of the last-known inputs, internal variables, and outputs.
 */
struct StateSnapshot {
    quint64                    seq;     /**< Monotonically increasing sequence number */
    qint64                     ts;      /**< Timestamp (ms since epoch) */
    QString                    state;   /**< Current state name */
    QMap<QString, QString>     inputs;  /**< Last-known input values */
    QMap<QString, QString>     vars;    /**< Last-known variable values */
    QMap<QString, QString>     outputs; /**< Last-known output values */
};
Q_DECLARE_METATYPE(StateSnapshot)

/**
 * @class RuntimeClient
 * @brief QObject that manages UDP communication with an FSM runtime.
 *
 * Creates and owns a UdpChannel for JSON‐based control and telemetry.
 * Spawns a worker thread with QTimer to poll incoming state updates
 * and emits Qt signals for the GUI to consume.
 */
class RuntimeClient : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Constructs the client with bind and peer endpoints.
     * @param bindAddr  Local address to bind (e.g. "0.0.0.0:45455").
     * @param peerAddr  Remote address to send to (e.g. "127.0.0.1:45454").
     * @param parent    Optional parent QObject.
     */
    explicit RuntimeClient(QString bindAddr,
                        QString peerAddr,
                        QObject* parent = nullptr);

    /**
     * @brief Cleans up the polling thread and channel.
     */
    ~RuntimeClient() override;

    /**
     * @brief Starts the polling thread and begins receiving state updates.
     *
     * Must be called once from the GUI thread before any other operations.
     */
    void start();

    /**
     * @brief Stops polling and closes the underlying UDP channel.
     */
    void stop();

    /**
     * @brief Gracefully requests the remote interpreter to shut down.
     */
    void shutdown();

    /**
     * @brief Sends a “setVar” command to update an internal variable.
     * @param name   Variable name.
     * @param value  New variable value.
     */
    void setVariable(QString name, QString value);

    /**
     * @brief Sends a custom JSON‐encoded message over UDP.
     * @param jsonMessage  Raw JSON string.
     */
    void sendCustomMessage(const std::string& jsonMessage);

signals:
    /**
     * @brief Emitted whenever a valid “state” packet is received.
     * @param snapshot  Parsed StateSnapshot.
     */
    void stateReceived(StateSnapshot snapshot);

    /**
     * @brief Emitted to log brief status or debug messages.
     * @param message  Text to append in the GUI console.
     */
    void logMessage(const QString& message);

private slots:
    /**
     * @brief Slot invoked when the worker thread starts:
     *        sets up the QTimer to call pollChannel().
     */
    void onThreadStarted();

    /**
     * @brief Polls the UDP socket for incoming packets and
     *        emits stateReceived/logMessage as appropriate.
     */
    void pollChannel();

private:
    QMap<QString,QString> m_prevInputs;
    QMap<QString,QString> m_prevOutputs;
    QMap<QString,QString> m_prevVars;
    bool                  m_hasPrev = false;
    const QString                              m_bindAddr;
    const QString                              m_peerAddr;
    std::shared_ptr<io_bridge::IChannel>       m_channel;  /**< Underlying UDP channel */
    QThread*                                   m_thread{nullptr};
    QTimer*                                    m_timer{nullptr};
};

#endif // UI_QT_RUNTIME_CLIENT_HPP
