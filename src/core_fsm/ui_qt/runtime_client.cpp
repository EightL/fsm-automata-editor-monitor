#include "runtime_client.hpp"
#include <nlohmann/json.hpp>
#include "../io_bridge/udp_channel.hpp"   // ← add this line

using namespace io_bridge;

RuntimeClient::RuntimeClient(QString bindAddr,
                             QString peerAddr,
                             QObject* parent)
  : QObject(parent)
  , m_bindAddr(std::move(bindAddr))
  , m_peerAddr(std::move(peerAddr))
{
    // Make the FSM state snapshot type known to Qt's meta‐system
    qRegisterMetaType<StateSnapshot>("StateSnapshot");
}

RuntimeClient::~RuntimeClient() {
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
    }
}

void RuntimeClient::start() {
    // 1) Create the UDP channel on caller thread
    m_channel = std::make_shared<UdpChannel>(
                    m_bindAddr.toStdString(),
                    m_peerAddr.toStdString());

    // 2) Spawn a thread and move ourselves into it
    m_thread = new QThread(this);
    connect(m_thread, &QThread::started,
            this,       &RuntimeClient::onThreadStarted);
    this->moveToThread(m_thread);
    m_thread->start();
}

void RuntimeClient::onThreadStarted() {
    // 3) Create a timer to poll the socket ~every 20ms
    m_timer = new QTimer(this);
    m_timer->setInterval(20);
    connect(m_timer, &QTimer::timeout,
            this,    &RuntimeClient::pollChannel);
    m_timer->start();
}

void RuntimeClient::pollChannel() {
    Packet p;
    while (m_channel && m_channel->poll(p)) {
        auto j = nlohmann::json::parse(p.json, nullptr, false);
        if (j.is_discarded() || j.value("type","") != "state")
            continue;

        StateSnapshot snap;
        snap.seq   = j.at("seq").get<quint64>();
        snap.ts    = j.at("ts").get<qint64>();
        snap.state = QString::fromStdString(j.at("state").get<std::string>());

        // helper to pull JSON object<string,string> → QMap
        auto jsonToMap = [&](const auto& node){
            QMap<QString,QString> map;
            for (auto it = node.begin(); it != node.end(); ++it) {
                map.insert(
                    QString::fromStdString(it.key()),
                    QString::fromStdString(it.value().template get<std::string>())
                );
            }
            return map;
        };

        snap.inputs  = jsonToMap(j.at("inputs"));
        snap.vars    = jsonToMap(j.at("vars"));
        snap.outputs = jsonToMap(j.at("outputs"));

        // emit into GUI thread (Qt::QueuedConnection by default)
        emit stateReceived(snap);
    }
}

void RuntimeClient::inject(QString name, QString value) {
    if (!m_channel) return;
    nlohmann::json j = {
        {"type",  "inject"},
        {"name",  name.toStdString()},
        {"value", value.toStdString()}
    };
    m_channel->send({ j.dump() });
}
