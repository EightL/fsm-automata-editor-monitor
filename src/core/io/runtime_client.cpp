/**
 * @file   runtime_client.cpp
 * @brief  Implements RuntimeClient: manages UDP channel, worker thread,
 *         message formatting, and state polling logic.
 *
 * Defines methods to start/stop the background poller, send control
 * JSON messages, and parse incoming “state” packets into StateSnapshot
 * objects for the GUI.
 *
 * @author Martin Ševčík (xsevcim00)
 * @author Jakub Lůčný
 * @date   2025-05-06
 */

 #include "runtime_client.hpp"
 #include <thread>
 using namespace io_bridge;
 
 // Constructor: register StateSnapshot with Qt's meta‐object system
 RuntimeClient::RuntimeClient(QString bindAddr,
                              QString peerAddr,
                              QObject* parent)
   : QObject(parent)
   , m_bindAddr(std::move(bindAddr))
   , m_peerAddr(std::move(peerAddr))
 {
     qRegisterMetaType<StateSnapshot>("StateSnapshot");
 }
 
 // Destructor: ensure the polling thread is cleanly stopped
 RuntimeClient::~RuntimeClient() {
     if (m_thread) {
         m_thread->quit();
         m_thread->wait();
     }
 }
 
 // Stops the worker thread and closes the UDP channel
 void RuntimeClient::stop()
 {
     if (m_thread) {
         m_thread->quit();
         m_thread->wait();
     }
     m_channel.reset();  // destructor of UdpChannel closes socket
     m_thread = nullptr;
 }
 
 // Starts the UDP channel and spawns the polling thread
 void RuntimeClient::start() {
     // 1) Create the UDP channel on the calling (GUI) thread
     m_channel = std::make_shared<UdpChannel>(
                     m_bindAddr.toStdString(),
                     m_peerAddr.toStdString());
 
     // 2) Move this object into a new QThread
     m_thread = new QThread(this);
     connect(m_thread, &QThread::started,
             this,       &RuntimeClient::onThreadStarted);
     this->moveToThread(m_thread);
     m_thread->start();
 }
 
 // Sends any arbitrary JSON message over UDP
 void RuntimeClient::sendCustomMessage(const std::string& jsonMessage) {
     io_bridge::Packet pkt;
     pkt.json = jsonMessage;
     m_channel->send(pkt);
 }
 
 // Formats and sends a setVar JSON command
 void RuntimeClient::setVariable(QString name, QString value) {
     if (!m_channel) return;
     nlohmann::json j = {
         {"type",  "setVar"},
         {"name",  name.toStdString()},
         {"value", value.toStdString()}
     };
     sendCustomMessage(j.dump());
 }
 
 // Sends a shutdown command, then waits briefly for the interpreter to exit
 void RuntimeClient::shutdown() {
     if (!m_channel) return;
     nlohmann::json j = { {"type","shutdown"} };
     m_channel->send({ j.dump() });
     std::this_thread::sleep_for(std::chrono::milliseconds(100));
 }
 
 // Slot: called when the thread starts; sets up the poll timer
 void RuntimeClient::onThreadStarted() {
     m_timer = new QTimer(this);
     m_timer->setInterval(20);  // poll every 20 ms
     connect(m_timer, &QTimer::timeout,
             this,    &RuntimeClient::pollChannel);
     m_timer->start();
 }


 // Polls the UDP channel; on valid “state” JSON, emits snapshot + log
 void RuntimeClient::pollChannel()
 {
     Packet p;
     if (!m_channel || !m_channel->poll(p))
         return;
 
     // ---------- parse packet ------------------------------------------------
     auto j = nlohmann::json::parse(p.json, nullptr, false);
     if (j.is_discarded() || j.value("type","") != "state")
         return;
 
     StateSnapshot snap;
     snap.seq   = j.at("seq").get<quint64>();
     snap.ts    = j.at("ts").get<qint64>();
     snap.state = QString::fromStdString(j.at("state").get<std::string>());
 
     // helper: JSON object → QMap<QString,QString>
     auto jsonToMap = [&](auto const& node) {
        QMap<QString,QString> map;
        for (auto it = node.begin(); it != node.end(); ++it) {
            QString val;
            if      (it.value().is_string())        val = QString::fromStdString(it.value().template get<std::string>());
            else if (it.value().is_number_integer()) val = QString::number(it.value().template get<qint64>());
            else if (it.value().is_number_float())   val = QString::number(it.value().template get<double>());
            else                                     val = QString::fromStdString(it.value().dump());
            map.insert(QString::fromStdString(it.key()), val);
        }
        return map;
     };
 
     snap.inputs  = jsonToMap(j.at("inputs"));
     snap.vars    = jsonToMap(j.at("vars"));
     snap.outputs = jsonToMap(j.at("outputs"));
 
     // ---------- NEW: diff against previous snapshot -------------------------
     auto reportChanges = [this](const char* tag,
                                 const QMap<QString,QString>& oldM,
                                 const QMap<QString,QString>& newM)
     {
         for (auto it = newM.constBegin(); it != newM.constEnd(); ++it) {
             if (it.value() != oldM.value(it.key()))
                 emit logMessage(QString("%1 %2 = %3")
                                 .arg(tag, it.key(), it.value()));
         }
     };
 
     if (m_hasPrev) {
         reportChanges("INPUT" , m_prevInputs , snap.inputs );
         reportChanges("OUTPUT", m_prevOutputs, snap.outputs);
         reportChanges("VAR"   , m_prevVars   , snap.vars   );
     }
     m_prevInputs  = snap.inputs;
     m_prevOutputs = snap.outputs;
     m_prevVars    = snap.vars;
     m_hasPrev     = true;
 
     // ---------- normal state‑change log & GUI update ------------------------
     emit logMessage(QString("STATE CHANGED: %1").arg(snap.state));
     emit stateReceived(snap);
 }
 