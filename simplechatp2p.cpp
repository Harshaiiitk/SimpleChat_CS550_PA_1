#include "simplechatp2p.h"
#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QInputDialog>

SimpleChatP2P::SimpleChatP2P(const QString& clientId, int port, QWidget *parent)
    : QMainWindow(parent)
    , m_centralWidget(nullptr)
    , m_mainLayout(nullptr)
    , m_chatLog(nullptr)
    , m_messageInput(nullptr)
    , m_sendButton(nullptr)
    , m_broadcastButton(nullptr)
    , m_destinationCombo(nullptr)
    , m_statusLabel(nullptr)
    , m_udpSocket(nullptr)
    , m_discoveryTimer(new QTimer(this))
    , m_antiEntropyTimer(new QTimer(this))
    , m_retransmissionTimer(new QTimer(this))
    , m_clientId(clientId)
    , m_port(port)
    , m_sequenceNumber(1)
{
    setupUI();
    setupNetwork();
    
    setWindowTitle(QString("SimpleChat P2P - %1 (Port %2)").arg(m_clientId).arg(m_port));
    resize(700, 600);
}

SimpleChatP2P::~SimpleChatP2P()
{
    if (m_udpSocket) {
        m_udpSocket->close();
    }
}

void SimpleChatP2P::setupUI()
{
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);
    
    m_mainLayout = new QVBoxLayout(m_centralWidget);
    
    // Status label
    m_statusLabel = new QLabel("Initializing P2P network...", this);
    m_mainLayout->addWidget(m_statusLabel);
    
    // Chat log area
    m_chatLog = new QTextEdit(this);
    m_chatLog->setReadOnly(true);
    m_chatLog->setWordWrapMode(QTextOption::WordWrap);
    m_mainLayout->addWidget(m_chatLog);
    
    // Peer management area
    QHBoxLayout* peerLayout = new QHBoxLayout();
    peerLayout->addWidget(new QLabel("Add Peer (IP:Port):"));
    m_peerAddressInput = new QLineEdit(this);
    m_peerAddressInput->setPlaceholderText("127.0.0.1:9001");
    peerLayout->addWidget(m_peerAddressInput);
    m_addPeerButton = new QPushButton("Add Peer", this);
    peerLayout->addWidget(m_addPeerButton);
    m_mainLayout->addLayout(peerLayout);
    
    // Input area
    m_inputLayout = new QHBoxLayout();
    
    // Destination selector
    m_destinationCombo = new QComboBox(this);
    m_destinationCombo->addItem("Select Peer...");
    m_inputLayout->addWidget(new QLabel("To:"));
    m_inputLayout->addWidget(m_destinationCombo);
    
    // Message input
    m_messageInput = new QLineEdit(this);
    m_messageInput->setPlaceholderText("Type your message here...");
    m_inputLayout->addWidget(m_messageInput, 1);
    
    // Send button
    m_sendButton = new QPushButton("Send", this);
    m_inputLayout->addWidget(m_sendButton);
    
    // Broadcast button
    m_broadcastButton = new QPushButton("Broadcast", this);
    m_inputLayout->addWidget(m_broadcastButton);
    
    m_mainLayout->addLayout(m_inputLayout);
    
    // Connect signals
    connect(m_sendButton, &QPushButton::clicked, this, &SimpleChatP2P::sendMessage);
    connect(m_messageInput, &QLineEdit::returnPressed, this, &SimpleChatP2P::sendMessage);
    connect(m_broadcastButton, &QPushButton::clicked, [this]() {
        QString messageText = m_messageInput->text().trimmed();
        if (messageText.isEmpty()) return;
        
        QVariantMap message;
        message["ChatText"] = messageText;
        message["Origin"] = m_clientId;
        message["Destination"] = "-1"; // Broadcast indicator
        message["Sequence"] = m_sequenceNumber++;
        message["Type"] = "message";
        message["Timestamp"] = QDateTime::currentMSecsSinceEpoch();
        
        addToMessageLog(QString("📢 Broadcast: %1").arg(messageText), m_clientId);
        broadcastMessage(message);
        
        // Store our own broadcast message
        MessageInfo info;
        info.origin = m_clientId;
        info.destination = "-1";
        info.chatText = messageText;
        info.sequence = message["Sequence"].toInt();
        info.timestamp = QDateTime::currentDateTime();
        storeMessage(info);
        
        m_messageInput->clear();
    });
    connect(m_addPeerButton, &QPushButton::clicked, this, &SimpleChatP2P::addPeerManually);
    
    // Auto-focus on message input
    m_messageInput->setFocus();
    
    addToMessageLog("Chat initialized. P2P mode with UDP.");
    addToMessageLog("Use 'Add Peer' to connect to other instances.");
}

void SimpleChatP2P::setupNetwork()
{
    // Create UDP socket
    m_udpSocket = new QUdpSocket(this);
    
    if (!m_udpSocket->bind(QHostAddress::Any, m_port)) {
        addToMessageLog(QString("Failed to bind to port %1: %2")
                       .arg(m_port).arg(m_udpSocket->errorString()));
        return;
    }
    
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &SimpleChatP2P::readPendingDatagrams);
    
    addToMessageLog(QString("UDP socket bound to port %1").arg(m_port));
    
    // Setup timers
    connect(m_discoveryTimer, &QTimer::timeout, this, &SimpleChatP2P::performPeerDiscovery);
    m_discoveryTimer->start(DISCOVERY_INTERVAL);
    
    connect(m_antiEntropyTimer, &QTimer::timeout, this, &SimpleChatP2P::performAntiEntropy);
    m_antiEntropyTimer->start(ANTI_ENTROPY_INTERVAL);
    
    connect(m_retransmissionTimer, &QTimer::timeout, this, &SimpleChatP2P::checkMessageRetransmission);
    m_retransmissionTimer->start(RETRANSMISSION_INTERVAL);
    
    m_statusLabel->setText(QString("Connected - %1 (UDP Port %2)").arg(m_clientId).arg(m_port));
    
    // Start initial peer discovery
    performPeerDiscovery();
}

void SimpleChatP2P::sendMessage()
{
    QString messageText = m_messageInput->text().trimmed();
    if (messageText.isEmpty()) {
        return;
    }
    
    QString destination = m_destinationCombo->currentText();
    if (destination == "Select Peer..." || destination.isEmpty()) {
        addToMessageLog("Please select a destination peer");
        return;
    }
    
    // Create message
    QVariantMap message;
    message["ChatText"] = messageText;
    message["Origin"] = m_clientId;
    message["Destination"] = destination;
    message["Sequence"] = m_sequenceNumber++;
    message["Type"] = "message";
    message["Timestamp"] = QDateTime::currentMSecsSinceEpoch();
    
    // Add to our own chat log
    addToMessageLog(QString("→ %1: %2").arg(destination, messageText), m_clientId);
    
    // Store message
    MessageInfo info;
    info.origin = m_clientId;
    info.destination = destination;
    info.chatText = messageText;
    info.sequence = message["Sequence"].toInt();
    info.timestamp = QDateTime::currentDateTime();
    storeMessage(info);
    
    // Send to destination peer
    if (m_peers.contains(destination)) {
        const PeerInfo& peer = m_peers[destination];
        sendMessageToPeer(message, peer.address, peer.port);
        
        // Add to pending acknowledgments for retransmission
        m_pendingAcks[m_clientId].insert(info.sequence);
    } else {
        addToMessageLog("Destination peer not found. Broadcasting...");
        broadcastMessage(message);
    }
    
    // Clear input
    m_messageInput->clear();
    m_messageInput->setFocus();
}

void SimpleChatP2P::readPendingDatagrams()
{
    while (m_udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_udpSocket->pendingDatagramSize());
        
        QHostAddress senderAddr;
        quint16 senderPort;
        
        m_udpSocket->readDatagram(datagram.data(), datagram.size(), &senderAddr, &senderPort);
        
        QVariantMap message = deserializeMessage(datagram);
        if (!message.isEmpty()) {
            processReceivedMessage(message, senderAddr, senderPort);
        }
    }
}

void SimpleChatP2P::processReceivedMessage(const QVariantMap& message, const QHostAddress& senderAddr, quint16 senderPort)
{
    QString type = message["Type"].toString();
    QString origin = message["Origin"].toString();
    
    // Update peer information
    if (!origin.isEmpty() && origin != m_clientId) {
        updatePeerLastSeen(senderAddr, senderPort);
        if (!m_peers.contains(origin)) {
            addPeer(origin, senderAddr, senderPort);
        }
    }
    
    if (type == "message") {
        QString destination = message["Destination"].toString();
        QString chatText = message["ChatText"].toString();
        int sequence = message["Sequence"].toInt();
        
        // Check if we've already seen this message
        if (hasMessage(origin, sequence)) {
            return; // Duplicate, ignore
        }
        
        // Store the message
        MessageInfo info;
        info.origin = origin;
        info.destination = destination;
        info.chatText = chatText;
        info.sequence = sequence;
        info.timestamp = QDateTime::currentDateTime();
        storeMessage(info);
        
        // Send acknowledgment
        QVariantMap ack;
        ack["Type"] = "ack";
        ack["Origin"] = m_clientId;
        ack["AckOrigin"] = origin;
        ack["AckSequence"] = sequence;
        sendMessageToPeer(ack, senderAddr, senderPort);
        
        // Display if for us or broadcast
        if (destination == m_clientId) {
            addToMessageLog(QString("← %1: %2").arg(origin, chatText), origin);
        } else if (destination == "-1") {
            addToMessageLog(QString("📢 %1: %2").arg(origin, chatText), origin);
        }
        
    } else if (type == "ack") {
        QString ackOrigin = message["AckOrigin"].toString();
        int ackSequence = message["AckSequence"].toInt();
        
        // Remove from pending acknowledgments
        if (ackOrigin == m_clientId) {
            m_pendingAcks[ackOrigin].remove(ackSequence);
        }
        
        // Track acknowledgment in message store
        if (m_messageStore.contains(ackOrigin) && m_messageStore[ackOrigin].contains(ackSequence)) {
            m_messageStore[ackOrigin][ackSequence].acknowledgedBy.insert(origin);
        }
        
    } else if (type == "discovery") {
        // Peer discovery response
        QVariantMap response;
        response["Type"] = "discovery_response";
        response["Origin"] = m_clientId;
        response["Port"] = m_port;
        sendMessageToPeer(response, senderAddr, senderPort);
        
    } else if (type == "discovery_response") {
        // Already handled by updatePeerLastSeen
        
    } else if (type == "vector_clock") {
        handleVectorClock(message, senderAddr, senderPort);
        
    } else if (type == "sync_message") {
        // Message received during anti-entropy sync
        QString syncOrigin = message["SyncOrigin"].toString();
        int syncSequence = message["SyncSequence"].toInt();
        QString syncDest = message["SyncDestination"].toString();
        QString syncText = message["SyncText"].toString();
        
        if (!hasMessage(syncOrigin, syncSequence)) {
            MessageInfo info;
            info.origin = syncOrigin;
            info.destination = syncDest;
            info.chatText = syncText;
            info.sequence = syncSequence;
            info.timestamp = QDateTime::currentDateTime();
            storeMessage(info);
            
            addToMessageLog(QString("🔄 Synced: %1 (seq %2)").arg(syncOrigin).arg(syncSequence));
        }
    }
}

void SimpleChatP2P::sendMessageToPeer(const QVariantMap& message, const QHostAddress& addr, quint16 port)
{
    QByteArray data = serializeMessage(message);
    m_udpSocket->writeDatagram(data, addr, port);
}

void SimpleChatP2P::broadcastMessage(const QVariantMap& message)
{
    for (const PeerInfo& peer : m_peers) {
        sendMessageToPeer(message, peer.address, peer.port);
    }
}

void SimpleChatP2P::performPeerDiscovery()
{
    // Discover peers on local ports
    QVariantMap discovery;
    discovery["Type"] = "discovery";
    discovery["Origin"] = m_clientId;
    discovery["Port"] = m_port;
    
    for (int port = BASE_PORT; port < BASE_PORT + MAX_PORTS; ++port) {
        if (port != m_port) {
            sendMessageToPeer(discovery, QHostAddress::LocalHost, port);
        }
    }
    
    // Clean up old peers
    QDateTime now = QDateTime::currentDateTime();
    QStringList toRemove;
    for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
        if (it->lastSeen.msecsTo(now) > PEER_TIMEOUT) {
            toRemove.append(it.key());
        }
    }
    
    for (const QString& peerId : toRemove) {
        m_peers.remove(peerId);
        addToMessageLog(QString("Peer %1 timed out").arg(peerId));
        
        // Update combo box
        int index = m_destinationCombo->findText(peerId);
        if (index >= 0) {
            m_destinationCombo->removeItem(index);
        }
    }
}

void SimpleChatP2P::performAntiEntropy()
{
    // Send vector clock to all peers
    for (const PeerInfo& peer : m_peers) {
        sendVectorClock(peer.address, peer.port);
    }
}

void SimpleChatP2P::sendVectorClock(const QHostAddress& addr, quint16 port)
{
    VectorClock myClock = getMyVectorClock();
    
    QVariantMap message;
    message["Type"] = "vector_clock";
    message["Origin"] = m_clientId;
    
    QVariantMap clockMap;
    for (auto it = myClock.sequences.begin(); it != myClock.sequences.end(); ++it) {
        clockMap[it.key()] = it.value();
    }
    message["VectorClock"] = clockMap;
    
    sendMessageToPeer(message, addr, port);
}

void SimpleChatP2P::handleVectorClock(const QVariantMap& message, const QHostAddress& addr, quint16 port)
{
    QVariantMap clockMap = message["VectorClock"].toMap();
    
    VectorClock peerClock;
    for (auto it = clockMap.begin(); it != clockMap.end(); ++it) {
        peerClock.sequences[it.key()] = it.value().toInt();
    }
    
    // Send missing messages
    sendMissingMessages(peerClock, addr, port);
}

void SimpleChatP2P::sendMissingMessages(const VectorClock& peerClock, const QHostAddress& addr, quint16 port)
{
    // For each origin in our message store
    for (auto originIt = m_messageStore.begin(); originIt != m_messageStore.end(); ++originIt) {
        QString origin = originIt.key();
        int peerMaxSeq = peerClock.sequences.value(origin, 0);
        
        // Send messages with sequence > peerMaxSeq
        for (auto seqIt = originIt->begin(); seqIt != originIt->end(); ++seqIt) {
            if (seqIt.key() > peerMaxSeq) {
                const MessageInfo& info = seqIt.value();
                
                QVariantMap syncMsg;
                syncMsg["Type"] = "sync_message";
                syncMsg["Origin"] = m_clientId;
                syncMsg["SyncOrigin"] = info.origin;
                syncMsg["SyncSequence"] = info.sequence;
                syncMsg["SyncDestination"] = info.destination;
                syncMsg["SyncText"] = info.chatText;
                
                sendMessageToPeer(syncMsg, addr, port);
            }
        }
    }
}

VectorClock SimpleChatP2P::getMyVectorClock() const
{
    VectorClock clock;
    
    for (auto it = m_messageStore.begin(); it != m_messageStore.end(); ++it) {
        QString origin = it.key();
        if (!it->isEmpty()) {
            // Get the maximum sequence number for this origin
            int maxSeq = (--it->end()).key();
            clock.sequences[origin] = maxSeq;
        }
    }
    
    return clock;
}

void SimpleChatP2P::checkMessageRetransmission()
{
    QDateTime now = QDateTime::currentDateTime();
    
    // Check messages needing retransmission
    for (auto originIt = m_pendingAcks.begin(); originIt != m_pendingAcks.end(); ++originIt) {
        QString origin = originIt.key();
        for (int seq : originIt.value()) {
            if (hasMessage(origin, seq)) {
                const MessageInfo& info = getMessage(origin, seq);
                
                // If message is older than 2 seconds and not fully acknowledged
                if (info.timestamp.msecsTo(now) > RETRANSMISSION_INTERVAL) {
                    // Retransmit
                    QVariantMap message;
                    message["ChatText"] = info.chatText;
                    message["Origin"] = info.origin;
                    message["Destination"] = info.destination;
                    message["Sequence"] = info.sequence;
                    message["Type"] = "message";
                    message["Timestamp"] = info.timestamp.toMSecsSinceEpoch();
                    
                    if (info.destination == "-1") {
                        broadcastMessage(message);
                    } else if (m_peers.contains(info.destination)) {
                        const PeerInfo& peer = m_peers[info.destination];
                        sendMessageToPeer(message, peer.address, peer.port);
                    }
                    
                    addToMessageLog(QString("🔄 Retransmitting seq %1").arg(seq));
                }
            }
        }
    }
}

void SimpleChatP2P::addPeerManually()
{
    QString address = m_peerAddressInput->text().trimmed();
    if (address.isEmpty()) return;
    
    QStringList parts = address.split(":");
    if (parts.size() != 2) {
        addToMessageLog("Invalid format. Use IP:Port (e.g., 127.0.0.1:9001)");
        return;
    }
    
    QString ip = parts[0];
    bool ok;
    int port = parts[1].toInt(&ok);
    
    if (!ok || port <= 0 || port > 65535) {
        addToMessageLog("Invalid port number");
        return;
    }
    
    QHostAddress addr(ip);
    if (addr.isNull()) {
        addToMessageLog("Invalid IP address");
        return;
    }
    
    // Send discovery to this specific peer
    QVariantMap discovery;
    discovery["Type"] = "discovery";
    discovery["Origin"] = m_clientId;
    discovery["Port"] = m_port;
    sendMessageToPeer(discovery, addr, port);
    
    addToMessageLog(QString("Sent discovery to %1:%2").arg(ip).arg(port));
    m_peerAddressInput->clear();
}

void SimpleChatP2P::addPeer(const QString& peerId, const QHostAddress& addr, quint16 port)
{
    if (peerId == m_clientId) return; // Don't add ourselves
    
    if (!m_peers.contains(peerId)) {
        PeerInfo info;
        info.address = addr;
        info.port = port;
        info.lastSeen = QDateTime::currentDateTime();
        info.peerId = peerId;
        
        m_peers[peerId] = info;
        
        // Add to combo box
        m_destinationCombo->addItem(peerId);
        
        addToMessageLog(QString("✅ Peer connected: %1 (%2:%3)")
                       .arg(peerId)
                       .arg(addr.toString())
                       .arg(port));
        
        m_statusLabel->setText(QString("Connected - %1 peers").arg(m_peers.size()));
    }
}

void SimpleChatP2P::updatePeerLastSeen(const QHostAddress& addr, quint16 port)
{
    for (auto& peer : m_peers) {
        if (peer.address == addr && peer.port == port) {
            peer.lastSeen = QDateTime::currentDateTime();
            break;
        }
    }
}

QList<SimpleChatP2P::PeerInfo> SimpleChatP2P::getActivePeers() const
{
    return m_peers.values();
}

void SimpleChatP2P::storeMessage(const MessageInfo& msgInfo)
{
    m_messageStore[msgInfo.origin][msgInfo.sequence] = msgInfo;
}

bool SimpleChatP2P::hasMessage(const QString& origin, int sequence) const
{
    return m_messageStore.contains(origin) && m_messageStore[origin].contains(sequence);
}

MessageInfo SimpleChatP2P::getMessage(const QString& origin, int sequence) const
{
    return m_messageStore[origin][sequence];
}

void SimpleChatP2P::addToMessageLog(const QString& text, const QString& sender)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString logEntry = QString("[%1] %2").arg(timestamp, text);
    
    m_chatLog->append(logEntry);
    m_chatLog->ensureCursorVisible();
}

QByteArray SimpleChatP2P::serializeMessage(const QVariantMap& message)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    
    stream << quint32(0xCAFEBABE); // Magic number
    stream << message;
    
    QByteArray result;
    QDataStream sizeStream(&result, QIODevice::WriteOnly);
    sizeStream.setVersion(QDataStream::Qt_6_0);
    sizeStream << quint32(data.size());
    result.append(data);
    
    return result;
}

QVariantMap SimpleChatP2P::deserializeMessage(const QByteArray& data)
{
    QVariantMap message;
    QDataStream stream(data);
    stream.setVersion(QDataStream::Qt_6_0);
    
    quint32 messageSize;
    stream >> messageSize;
    if (stream.status() != QDataStream::Ok) {
        return message;
    }
    
    quint32 magic;
    stream >> magic;
    if (stream.status() != QDataStream::Ok || magic != 0xCAFEBABE) {
        return message;
    }
    
    stream >> message;
    if (stream.status() != QDataStream::Ok) {
        return QVariantMap();
    }
    
    return message;
}