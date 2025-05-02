#include "runtime_client.hpp"
#include "../../../external/nlohmann/json.hpp" // for nlohmann::json
#include "../io_bridge/udp_channel.hpp"   // ← add this line
#include <QDebug>
#include <thread>
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

void RuntimeClient::sendCustomMessage(const std::string& jsonMessage) {
    io_bridge::Packet pkt;
    pkt.json = jsonMessage;
    m_channel->send(pkt);
}

void RuntimeClient::setVariable(QString name, QString value) {
    if (!m_channel) return;
    nlohmann::json j = {
        {"type",  "setVar"},
        {"name",  name.toStdString()},
        {"value", value.toStdString()}
    };
    sendCustomMessage(j.dump());
}

void RuntimeClient::onThreadStarted() {
    qDebug() << "⏱  poll timer starting in thread:" << QThread::currentThread();
    // 3) Create a timer to poll the socket ~every 20ms
    m_timer = new QTimer(this);
    m_timer->setInterval(20);
    connect(m_timer, &QTimer::timeout,
            this,    &RuntimeClient::pollChannel);
    m_timer->start();
}

void RuntimeClient::pollChannel() {
    Packet p;
    if (m_channel && m_channel->poll(p)) {
        qDebug() << "[RC] got one packet:" << QString::fromStdString(p.json);
        
        auto j = nlohmann::json::parse(p.json, nullptr, false);
        if (j.is_discarded() || j.value("type","") != "state")
            return;

        StateSnapshot snap;
        snap.seq   = j.at("seq").get<quint64>();
        snap.ts    = j.at("ts").get<qint64>();
        snap.state = QString::fromStdString(j.at("state").get<std::string>());

        // helper to pull JSON object<string,string> → QMap
        auto jsonToMap = [&](const auto& node){
            QMap<QString,QString> map;
            for (auto it = node.begin(); it != node.end(); ++it) {
                QString val;
                if      (it.value().is_string())        val = QString::fromStdString(it.value().template get<std::string>());
                else if (it.value().is_number_integer()) val = QString::number(it.value().template get<qint64>());
                else if (it.value().is_number_float())   val = QString::number(it.value().template get<double>());
                else                                     val = QString::fromStdString(it.value().dump());

                map.insert(
                  QString::fromStdString(it.key()),
                  val
                );
            }
            return map;
        };

        snap.inputs  = jsonToMap(j.at("inputs"));
        snap.vars    = jsonToMap(j.at("vars"));
        snap.outputs = jsonToMap(j.at("outputs"));

        qDebug() << "[RC] emitting stateReceived for seq" << snap.seq;
        emit stateReceived(snap);
    }
}

void RuntimeClient::inject(QString name, QString value) {
    printf("Injecting %s = %s\n", name.toStdString().c_str(), value.toStdString().c_str());
    if (!m_channel) return;
    nlohmann::json j = {
        {"type",  "inject"},
        {"name",  name.toStdString()},
        {"value", value.toStdString()}
    };
    printf("sendinggg\n");
    m_channel->send({ j.dump() });
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
}
