#include "simplechatp2p.h"
#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QInputDialog>
#include <QListWidgetItem>
#include <QRandomGenerator>

SimpleChatP2P::SimpleChatP2P(const QString& clientId, int port, QWidget *parent, bool noForward)
    : QMainWindow(parent)
    , m_centralWidget(nullptr)
    , m_mainLayout(nullptr)
    , m_chatLog(nullptr)
    , m_messageInput(nullptr)
    , m_sendButton(nullptr)
    , m_broadcastButton(nullptr)
    , m_privateButton(nullptr)
    , m_destinationCombo(nullptr)
    , m_statusLabel(nullptr)
    , m_nodeListWidget(nullptr)
    , m_udpSocket(nullptr)
    , m_discoveryTimer(new QTimer(this))
    , m_antiEntropyTimer(new QTimer(this))
    , m_retransmissionTimer(new QTimer(this))
    , m_routeRumorTimer(new QTimer(this))
    , m_clientId(clientId)
    , m_port(port)
    , m_sequenceNumber(1)
    , m_dsdvSequenceNumber(1)
    , m_noForwardMode(noForward)
{
    setupUI();
    setupNetwork();
    
    setWindowTitle(QString("SimpleChat P2P - %1 (Port %2)%3")
                   .arg(m_clientId)
                   .arg(m_port)
                   .arg(m_noForwardMode ? " [NO-FORWARD]" : ""));
    resize(900, 700);
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
    
    // Horizontal layout for chat and node list
    QHBoxLayout* contentLayout = new QHBoxLayout();
    
    // Chat log area (left side)
    QVBoxLayout* chatLayout = new QVBoxLayout();
    m_chatLog = new QTextEdit(this);
    m_chatLog->setReadOnly(true);
    m_chatLog->setWordWrapMode(QTextOption::WordWrap);
    chatLayout->addWidget(new QLabel("Chat Log:"));
    chatLayout->addWidget(m_chatLog);
    
    // Node list area (right side)
    QVBoxLayout* nodeLayout = new QVBoxLayout();
    nodeLayout->addWidget(new QLabel("Available Nodes:"));
    m_nodeListWidget = new QListWidget(this);
    m_nodeListWidget->setMaximumWidth(200);
    nodeLayout->addWidget(m_nodeListWidget);
    
    contentLayout->addLayout(chatLayout, 3);  // Chat takes 3/4 width
    contentLayout->addLayout(nodeLayout, 1);  // Node list takes 1/4 width
    m_mainLayout->addLayout(contentLayout);
    
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
    
    // Private Message button
    m_privateButton = new QPushButton("Private Msg", this);
    m_inputLayout->addWidget(m_privateButton);
    
    m_mainLayout->addLayout(m_inputLayout);
    
    // Connect signals
    connect(m_sendButton, &QPushButton::clicked, this, &SimpleChatP2P::sendMessage);
    connect(m_messageInput, &QLineEdit::returnPressed, this, &SimpleChatP2P::sendMessage);
    connect(m_privateButton, &QPushButton::clicked, this, &SimpleChatP2P::sendPrivateMessage);
    connect(m_nodeListWidget, &QListWidget::itemDoubleClicked, [this](QListWidgetItem* item) {
        QString destination = item->text().split(" ")[0];  // Get node ID from list item
        m_destinationCombo->setCurrentText(destination);
        sendPrivateMessage();
    });
    
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
    if (m_noForwardMode) {
        addToMessageLog("Running in NO-FORWARD mode (rendezvous server)");
    }
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
    
    // Setup route rumor timer for DSDV
    connect(m_routeRumorTimer, &QTimer::timeout, this, &SimpleChatP2P::sendRouteRumor);
    m_routeRumorTimer->start(ROUTE_RUMOR_INTERVAL);
    
    m_statusLabel->setText(QString("Connected - %1 (UDP Port %2)%3")
                          .arg(m_clientId)
                          .arg(m_port)
                          .arg(m_noForwardMode ? " [NO-FORWARD]" : ""));
    
    // Start initial peer discovery and send initial route rumor
    performPeerDiscovery();
    sendRouteRumor();  // Send initial route announcement
}

void SimpleChatP2P::sendPrivateMessage()
{
    QString messageText = m_messageInput->text().trimmed();
    if (messageText.isEmpty()) {
        return;
    }
    
    QString destination = m_destinationCombo->currentText();
    if (destination == "Select Peer..." || destination.isEmpty()) {
        // Try to get from selected node in list
        auto selectedItems = m_nodeListWidget->selectedItems();
        if (selectedItems.isEmpty()) {
            addToMessageLog("Please select a destination node");
            return;
        }
        destination = selectedItems.first()->text().split(" ")[0];
    }
    
    // Create private message with hop limit
    QVariantMap message;
    message["Dest"] = destination;
    message["Origin"] = m_clientId;
    message["ChatText"] = messageText;
    message["HopLimit"] = static_cast<quint32>(DEFAULT_HOP_LIMIT);
    message["Type"] = "private";
    message["Sequence"] = m_sequenceNumber++;
    
    // Add NAT traversal information
    message["LastIP"] = m_udpSocket->localAddress().toString();
    message["LastPort"] = m_port;
    
    addToMessageLog(QString("→ Private to %1: %2").arg(destination, messageText), m_clientId);
    
    // Check if we have a route to the destination
    if (m_routingTable.contains(destination)) {
        const RouteEntry& route = m_routingTable[destination];
        sendMessageToPeer(message, route.nextHop, route.nextPort);
        addToMessageLog(QString("Routing via %1:%2").arg(route.nextHop.toString()).arg(route.nextPort));
    } else {
        // No route found, broadcast to discover route
        addToMessageLog("No route to destination, broadcasting...");
        broadcastMessage(message);
    }
    
    m_messageInput->clear();
    m_messageInput->setFocus();
}

void SimpleChatP2P::sendRouteRumor()
{
    // Create route rumor message
    QVariantMap routeRumor;
    routeRumor["Type"] = "route_rumor";
    routeRumor["Origin"] = m_clientId;
    routeRumor["SeqNo"] = m_dsdvSequenceNumber++;
    
    // Add NAT traversal information
    routeRumor["LastIP"] = m_udpSocket->localAddress().toString();
    routeRumor["LastPort"] = m_port;
    
    // Send to random neighbor
    auto peers = getActivePeers();
    if (!peers.isEmpty()) {
        int randomIndex = QRandomGenerator::global()->bounded(peers.size());
        const PeerInfo& peer = peers.at(randomIndex);
        sendMessageToPeer(routeRumor, peer.address, peer.port);
        
        addToMessageLog(QString("Sent route rumor (seq %1) to %2")
                       .arg(routeRumor["SeqNo"].toInt())
                       .arg(peer.peerId));
    }
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
    
    // Add NAT traversal information
    message["LastIP"] = m_udpSocket->localAddress().toString();
    message["LastPort"] = m_port;
    
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
    
    // Send to destination peer using DSDV routing if available
    if (m_routingTable.contains(destination)) {
        const RouteEntry& route = m_routingTable[destination];
        sendMessageToPeer(message, route.nextHop, route.nextPort);
        addToMessageLog(QString("Using DSDV route via %1:%2")
                       .arg(route.nextHop.toString())
                       .arg(route.nextPort));
    } else if (m_peers.contains(destination)) {
        // Fall back to direct send if peer is known
        const PeerInfo& peer = m_peers[destination];
        sendMessageToPeer(message, peer.address, peer.port);
    } else {
        addToMessageLog("Destination peer not found. Broadcasting...");
        broadcastMessage(message);
    }
    
    // Add to pending acknowledgments for retransmission
    m_pendingAcks[m_clientId].insert(info.sequence);
    
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
    
    // Process NAT information if present
    if (message.contains("LastIP") && message.contains("LastPort")) {
        processNATInfo(message, senderAddr, senderPort);
    }
    
    // Update peer information
    if (!origin.isEmpty() && origin != m_clientId) {
        updatePeerLastSeen(senderAddr, senderPort);
        if (!m_peers.contains(origin)) {
            addPeer(origin, senderAddr, senderPort);
        }
    }
    
    if (type == "route_rumor") {
        processRouteRumor(message, senderAddr, senderPort);
    } else if (type == "private") {
        // Handle private messages with DSDV routing
        QString dest = message["Dest"].toString();
        
        if (dest == m_clientId) {
            // Message is for us
            QString chatText = message["ChatText"].toString();
            addToMessageLog(QString("← Private from %1: %2").arg(origin, chatText), origin);
        } else {
            // Forward the message if not in no-forward mode
            if (!m_noForwardMode) {
                forwardPrivateMessage(message);
            }
        }
    } else if (type == "message") {
        // Check if in no-forward mode
        if (m_noForwardMode && message.contains("ChatText")) {
            // Don't forward chat messages in no-forward mode
            return;
        }
        
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
        
        // Update DSDV routing table based on this message
        updateRoutingTable(origin, senderAddr, senderPort, sequence, 1, true);
        
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
        response["LastIP"] = m_udpSocket->localAddress().toString();
        response["LastPort"] = m_port;
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

void SimpleChatP2P::processRouteRumor(const QVariantMap& message, const QHostAddress& senderAddr, quint16 senderPort)
{
    QString origin = message["Origin"].toString();
    int seqNo = message["SeqNo"].toInt();
    
    // Check if this is a new route rumor
    if (seqNo > m_lastSeqNoSeen[origin]) {
        m_lastSeqNoSeen[origin] = seqNo;
        
        // Update routing table
        updateRoutingTable(origin, senderAddr, senderPort, seqNo, 1, true);
        
        // Forward to random neighbor (rumor propagation)
        auto peers = getActivePeers();
        if (!peers.isEmpty()) {
            int randomIndex = QRandomGenerator::global()->bounded(peers.size());
            const PeerInfo& peer = peers.at(randomIndex);
            
            // Don't send back to sender
            if (peer.address != senderAddr || peer.port != senderPort) {
                sendMessageToPeer(message, peer.address, peer.port);
                addToMessageLog(QString("Forwarded route rumor from %1 (seq %2) to %3")
                               .arg(origin).arg(seqNo).arg(peer.peerId));
            }
        }
    }
}

void SimpleChatP2P::updateRoutingTable(const QString& destination, const QHostAddress& nextHop, 
                                      quint16 nextPort, int seqNo, int hopCount, bool isDirect)
{
    RouteEntry newRoute;
    newRoute.nextHop = nextHop;
    newRoute.nextPort = nextPort;
    newRoute.sequenceNumber = seqNo;
    newRoute.hopCount = hopCount;
    newRoute.lastUpdate = QDateTime::currentDateTime();
    newRoute.isDirect = isDirect;
    
    // Check for public endpoints if available
    if (m_publicEndpoints.contains(destination)) {
        auto [publicIP, publicPort] = m_publicEndpoints[destination];
        newRoute.publicIP = publicIP;
        newRoute.publicPort = publicPort;
    }
    
    // Check if we should update the route
    if (!m_routingTable.contains(destination)) {
        m_routingTable[destination] = newRoute;
        addToMessageLog(QString("New route to %1 via %2:%3 (seq %4)")
                       .arg(destination)
                       .arg(nextHop.toString())
                       .arg(nextPort)
                       .arg(seqNo));
        updateNodeList();
    } else {
        RouteEntry& oldRoute = m_routingTable[destination];
        if (isBetterRoute(oldRoute, newRoute)) {
            m_routingTable[destination] = newRoute;
            addToMessageLog(QString("Updated route to %1 via %2:%3 (seq %4)")
                           .arg(destination)
                           .arg(nextHop.toString())
                           .arg(nextPort)
                           .arg(seqNo));
            updateNodeList();
        }
    }
}

bool SimpleChatP2P::isBetterRoute(const RouteEntry& oldRoute, const RouteEntry& newRoute)
{
    // Prefer higher sequence numbers (fresher routes)
    if (newRoute.sequenceNumber > oldRoute.sequenceNumber) {
        return true;
    }
    
    // If same sequence number, prefer direct routes (for NAT traversal)
    if (newRoute.sequenceNumber == oldRoute.sequenceNumber && newRoute.isDirect && !oldRoute.isDirect) {
        return true;
    }
    
    // If same sequence number and directness, prefer shorter hop count
    if (newRoute.sequenceNumber == oldRoute.sequenceNumber && 
        newRoute.isDirect == oldRoute.isDirect && 
        newRoute.hopCount < oldRoute.hopCount) {
        return true;
    }
    
    return false;
}

void SimpleChatP2P::forwardPrivateMessage(const QVariantMap& message)
{
    QString dest = message["Dest"].toString();
    quint32 hopLimit = message["HopLimit"].toUInt();
    
    if (hopLimit > 0) {
        // Decrement hop limit
        QVariantMap forwardMsg = message;
        forwardMsg["HopLimit"] = hopLimit - 1;
        
        // Update NAT traversal info
        forwardMsg["LastIP"] = m_udpSocket->localAddress().toString();
        forwardMsg["LastPort"] = m_port;
        
        // Check routing table for destination
        if (m_routingTable.contains(dest)) {
            const RouteEntry& route = m_routingTable[dest];
            sendMessageToPeer(forwardMsg, route.nextHop, route.nextPort);
            addToMessageLog(QString("Forwarding private message to %1 via %2:%3")
                           .arg(dest)
                           .arg(route.nextHop.toString())
                           .arg(route.nextPort));
        } else {
            // No route found, broadcast to neighbors
            broadcastMessage(forwardMsg);
            addToMessageLog(QString("Broadcasting private message for %1 (no route)").arg(dest));
        }
    } else {
        addToMessageLog(QString("Dropped private message to %1 (hop limit reached)").arg(dest));
    }
}

void SimpleChatP2P::processNATInfo(const QVariantMap& message, const QHostAddress& senderAddr, quint16 senderPort)
{
    QString origin = message["Origin"].toString();
    
    // The sender's public endpoint is what we see
    if (!origin.isEmpty() && origin != m_clientId) {
        // Store the public endpoint we observed
        addPublicEndpoint(origin, senderAddr, senderPort);
        
        // If the message contains LastIP/LastPort different from what we see,
        // the sender is behind NAT
        QString lastIP = message["LastIP"].toString();
        quint16 lastPort = message["LastPort"].toUInt();
        
        QHostAddress reportedAddr(lastIP);

        // Check if this is a meaningful NAT detection (not just 0.0.0.0 or localhost differences)
        // and only log once per node
        bool isRealNAT = (reportedAddr != senderAddr || lastPort != senderPort);
        bool isNotLocalhost = !reportedAddr.isLoopback() && !senderAddr.isLoopback();
        bool isNotAnyAddress = !reportedAddr.isNull() && lastIP != "0.0.0.0";

        if (isRealNAT && isNotAnyAddress && !m_natDetected.contains(origin)) {
            // Only log meaningful NAT scenarios and only once per node
            if (isNotLocalhost) {
                addToMessageLog(QString("NAT detected for %1: local %2:%3 → public %4:%5")
                            .arg(origin)
                            .arg(lastIP).arg(lastPort)
                            .arg(senderAddr.toString()).arg(senderPort));
                m_natDetected.insert(origin);
            }
        }
    }
}

void SimpleChatP2P::addPublicEndpoint(const QString& nodeId, const QHostAddress& publicIP, quint16 publicPort)
{
    m_publicEndpoints[nodeId] = qMakePair(publicIP, publicPort);
    
    // Update routing table if we have a route to this node
    if (m_routingTable.contains(nodeId)) {
        m_routingTable[nodeId].publicIP = publicIP;
        m_routingTable[nodeId].publicPort = publicPort;
    }
}

void SimpleChatP2P::updateNodeList()
{
    m_nodeListWidget->clear();
    
    for (auto it = m_routingTable.begin(); it != m_routingTable.end(); ++it) {
        QString nodeId = it.key();
        const RouteEntry& route = it.value();
        
        QString displayText = QString("%1 (seq:%2, hop:%3)")
                             .arg(nodeId)
                             .arg(route.sequenceNumber)
                             .arg(route.hopCount);
        
        if (route.isDirect) {
            displayText += " [D]";
        }
        
        if (!route.publicIP.isNull()) {
            displayText += " [NAT]";
        }
        
        m_nodeListWidget->addItem(displayText);
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
    discovery["LastIP"] = m_udpSocket->localAddress().toString();
    discovery["LastPort"] = m_port;
    
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
        
        // Remove from routing table
        if (m_routingTable.contains(peerId)) {
            m_routingTable.remove(peerId);
            updateNodeList();
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
                    } else if (m_routingTable.contains(info.destination)) {
                        const RouteEntry& route = m_routingTable[info.destination];
                        sendMessageToPeer(message, route.nextHop, route.nextPort);
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
    discovery["LastIP"] = m_udpSocket->localAddress().toString();
    discovery["LastPort"] = m_port;
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
        
        // Update routing table with direct route
        updateRoutingTable(peerId, addr, port, 0, 1, true);
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