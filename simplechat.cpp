#include "simplechat.h"
#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>

// Static member definitions
const QStringList SimpleChat::s_peerIds = {"Client1", "Client2", "Client3", "Client4"};
const QList<int> SimpleChat::s_peerPorts = {9001, 9002, 9003, 9004};

SimpleChat::SimpleChat(const QString& clientId, int listenPort, int targetPort, QWidget *parent)
    : QMainWindow(parent)
    , m_centralWidget(nullptr)
    , m_mainLayout(nullptr)
    , m_chatLog(nullptr)
    , m_messageInput(nullptr)
    , m_sendButton(nullptr)
    , m_destinationCombo(nullptr)
    , m_statusLabel(nullptr)
    , m_server(nullptr)
    , m_nextPeerSocket(nullptr)
    , m_queueTimer(new QTimer(this))
    , m_clientId(clientId)
    , m_listenPort(listenPort)
    , m_targetPort(targetPort)
    , m_sequenceNumber(1)
{
    setupUI();
    setupNetwork();
    
    // Setup message queue processing
    connect(m_queueTimer, &QTimer::timeout, this, &SimpleChat::processMessageQueue);
    m_queueTimer->start(100); // Process queue every 100ms
    
    setWindowTitle(QString("SimpleChat - %1 (Port %2)").arg(m_clientId).arg(m_listenPort));
    resize(600, 500);
}

SimpleChat::~SimpleChat()
{
    if (m_nextPeerSocket && m_nextPeerSocket->state() == QAbstractSocket::ConnectedState) {
        m_nextPeerSocket->close();
    }
    if (m_server && m_server->isListening()) {
        m_server->close();
    }
}

void SimpleChat::setupUI()
{
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);
    
    m_mainLayout = new QVBoxLayout(m_centralWidget);
    
    // Status label
    m_statusLabel = new QLabel("Initializing...", this);
    m_mainLayout->addWidget(m_statusLabel);
    
    // Chat log area
    m_chatLog = new QTextEdit(this);
    m_chatLog->setReadOnly(true);
    m_chatLog->setWordWrapMode(QTextOption::WordWrap);
    m_mainLayout->addWidget(m_chatLog);
    
    // Input area
    m_inputLayout = new QHBoxLayout();
    
    // Destination selector
    m_destinationCombo = new QComboBox(this);
    for (const QString& peerId : s_peerIds) {
        if (peerId != m_clientId) {
            m_destinationCombo->addItem(peerId);
        }
    }
    m_inputLayout->addWidget(new QLabel("To:"));
    m_inputLayout->addWidget(m_destinationCombo);
    
    // Message input
    m_messageInput = new QLineEdit(this);
    m_messageInput->setPlaceholderText("Type your message here...");
    m_inputLayout->addWidget(m_messageInput, 1);
    
    // Send button
    m_sendButton = new QPushButton("Send", this);
    m_inputLayout->addWidget(m_sendButton);
    
    m_mainLayout->addLayout(m_inputLayout);
    
    // Connect signals
    connect(m_sendButton, &QPushButton::clicked, this, &SimpleChat::sendMessage);
    connect(m_messageInput, &QLineEdit::returnPressed, this, &SimpleChat::sendMessage);
    
    // Auto-focus on message input
    m_messageInput->setFocus();
    
    addToMessageLog("Chat initialized. Ready to send messages.");
}

void SimpleChat::setupNetwork()
{
    // Setup TCP server to listen for incoming connections
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &SimpleChat::onNewConnection);
    
    if (!m_server->listen(QHostAddress::LocalHost, m_listenPort)) {
        addToMessageLog(QString("Failed to start server on port %1: %2")
                       .arg(m_listenPort).arg(m_server->errorString()));
        return;
    }
    
    addToMessageLog(QString("Server listening on port %1").arg(m_listenPort));
    
    // Setup connection to next peer in ring
    m_nextPeerSocket = new QTcpSocket(this);
    connect(m_nextPeerSocket, &QTcpSocket::readyRead, this, &SimpleChat::onReadyRead);
    connect(m_nextPeerSocket, &QTcpSocket::disconnected, this, &SimpleChat::onDisconnected);
    
    // Try to connect to next peer
    QTimer::singleShot(1000, this, &SimpleChat::connectToNext); // Delay to allow other peers to start
}

void SimpleChat::connectToNext()
{
    if (m_nextPeerSocket->state() == QAbstractSocket::ConnectedState) {
        return; // Already connected
    }
    
    addToMessageLog(QString("Attempting to connect to next peer on port %1...").arg(m_targetPort));
    m_nextPeerSocket->connectToHost(QHostAddress::LocalHost, m_targetPort);
    
    // Update status
    if (m_nextPeerSocket->waitForConnected(3000)) {
        addToMessageLog("Connected to next peer in ring");
        m_statusLabel->setText(QString("Connected - %1:%2 → %3")
                              .arg(m_clientId).arg(m_listenPort).arg(m_targetPort));
    } else {
        addToMessageLog("Failed to connect to next peer. Will retry...");
        m_statusLabel->setText("Disconnected - Retrying...");
        // Retry connection after 5 seconds
        QTimer::singleShot(5000, this, &SimpleChat::connectToNext);
    }
}

void SimpleChat::sendMessage()
{
    QString messageText = m_messageInput->text().trimmed();
    if (messageText.isEmpty()) {
        return;
    }
    
    QString destination = m_destinationCombo->currentText();
    
    // Create message
    QVariantMap message;
    message["ChatText"] = messageText;
    message["Origin"] = m_clientId;
    message["Destination"] = destination;
    message["Sequence"] = m_sequenceNumber++;
    
    // Add to our own chat log
    addToMessageLog(QString("→ %1: %2").arg(destination, messageText), m_clientId);
    
    // Send to ring
    sendMessageToRing(message);
    
    // Clear input
    m_messageInput->clear();
    m_messageInput->setFocus();
}

void SimpleChat::sendMessageToRing(const QVariantMap& message)
{
    if (!m_nextPeerSocket || m_nextPeerSocket->state() != QAbstractSocket::ConnectedState) {
        addToMessageLog("Error: Not connected to ring network");
        return;
    }
    
    QByteArray data = serializeMessage(message);
    m_nextPeerSocket->write(data);
    m_nextPeerSocket->flush();
}

void SimpleChat::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket* clientSocket = m_server->nextPendingConnection();
        m_incomingConnections.append(clientSocket);
        
        connect(clientSocket, &QTcpSocket::readyRead, this, &SimpleChat::onReadyRead);
        connect(clientSocket, &QTcpSocket::disconnected, this, &SimpleChat::onDisconnected);
        
        addToMessageLog(QString("New connection from %1:%2")
                       .arg(clientSocket->peerAddress().toString())
                       .arg(clientSocket->peerPort()));
    }
}

void SimpleChat::onReadyRead()
{
    QTcpSocket* senderSocket = qobject_cast<QTcpSocket*>(sender());
    if (!senderSocket) return;
    
    while (senderSocket->bytesAvailable() > 0) {
        QByteArray data = senderSocket->readAll();
        QVariantMap message = deserializeMessage(data);
        
        if (!message.isEmpty()) {
            m_messageQueue.push(message);
        }
    }
}

void SimpleChat::processMessageQueue()
{
    while (!m_messageQueue.empty()) {
        QVariantMap message = m_messageQueue.front();
        m_messageQueue.pop();
        processReceivedMessage(message);
    }
}

void SimpleChat::processReceivedMessage(const QVariantMap& message)
{
    QString destination = message["Destination"].toString();
    QString origin = message["Origin"].toString();
    QString chatText = message["ChatText"].toString();
    int sequence = message["Sequence"].toInt();
    
    // Check if message is for us
    if (destination == m_clientId) {
        addToMessageLog(QString("← %1: %2").arg(origin, chatText), origin);
    } else {
        // Forward message to next peer in ring
        sendMessageToRing(message);
        addToMessageLog(QString("Forwarding message from %1 to %2 (seq: %3)")
                       .arg(origin, destination).arg(sequence));
    }
}

void SimpleChat::onDisconnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket == m_nextPeerSocket) {
        addToMessageLog("Disconnected from next peer. Attempting to reconnect...");
        m_statusLabel->setText("Disconnected - Reconnecting...");
        QTimer::singleShot(2000, this, &SimpleChat::connectToNext);
    } else {
        m_incomingConnections.removeAll(socket);
        socket->deleteLater();
        addToMessageLog("Incoming connection closed");
    }
}

void SimpleChat::addToMessageLog(const QString& text, const QString& sender)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString logEntry;
    
    if (sender.isEmpty()) {
        logEntry = QString("[%1] %2").arg(timestamp, text);
    } else {
        logEntry = QString("[%1] %2").arg(timestamp, text);
    }
    
    m_chatLog->append(logEntry);
    m_chatLog->ensureCursorVisible();
}

QByteArray SimpleChat::serializeMessage(const QVariantMap& message)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    
    // Write magic header to identify our messages
    stream << quint32(0xDEADBEEF);
    
    // Write message data
    stream << message;
    
    // Write size at the beginning
    QByteArray result;
    QDataStream sizeStream(&result, QIODevice::WriteOnly);
    sizeStream.setVersion(QDataStream::Qt_6_0);
    sizeStream << quint32(data.size());
    result.append(data);
    
    return result;
}

QVariantMap SimpleChat::deserializeMessage(const QByteArray& data)
{
    QVariantMap message;
    QDataStream stream(data);
    stream.setVersion(QDataStream::Qt_6_0);
    
    // Read size
    quint32 messageSize;
    stream >> messageSize;
    if (stream.status() != QDataStream::Ok) {
        return message; // Invalid data
    }
    
    // Read magic header
    quint32 magic;
    stream >> magic;
    if (stream.status() != QDataStream::Ok || magic != 0xDEADBEEF) {
        return message; // Invalid message
    }
    
    // Read message data
    stream >> message;
    if (stream.status() != QDataStream::Ok) {
        return QVariantMap(); // Return empty map on error
    }
    
    return message;
}