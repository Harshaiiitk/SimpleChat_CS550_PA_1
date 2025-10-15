#ifndef SIMPLECHAT_P2P_H
#define SIMPLECHAT_P2P_H

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtNetwork/QUdpSocket>
#include <QTimer>
#include <QMap>
#include <QSet>
#include <QVariantMap>
#include <QDateTime>

// Structure to hold message information
struct MessageInfo {
    QString origin;
    QString destination;
    QString chatText;
    int sequence;
    QDateTime timestamp;
    QSet<QString> acknowledgedBy; // Track which peers have acknowledged
};

// Vector clock for anti-entropy
struct VectorClock {
    QMap<QString, int> sequences; // origin -> highest sequence number seen
};

class SimpleChatP2P : public QMainWindow
{
    Q_OBJECT

public:
    SimpleChatP2P(const QString& clientId, int port, QWidget *parent = nullptr);
    ~SimpleChatP2P();

private slots:
    void sendMessage();
    void readPendingDatagrams();
    void performPeerDiscovery();
    void performAntiEntropy();
    void checkMessageRetransmission();
    void addPeerManually();

private:
    // UI Setup
    void setupUI();
    void setupNetwork();
    void addToMessageLog(const QString& text, const QString& sender = "");
    
    // Message handling
    void processReceivedMessage(const QVariantMap& message, const QHostAddress& senderAddr, quint16 senderPort);
    void sendMessageToPeer(const QVariantMap& message, const QHostAddress& addr, quint16 port);
    void broadcastMessage(const QVariantMap& message);
    
    // Message storage
    void storeMessage(const MessageInfo& msgInfo);
    bool hasMessage(const QString& origin, int sequence) const;
    MessageInfo getMessage(const QString& origin, int sequence) const;
    
    // Serialization
    QByteArray serializeMessage(const QVariantMap& message);
    QVariantMap deserializeMessage(const QByteArray& data);
    
    // Anti-entropy
    void sendVectorClock(const QHostAddress& addr, quint16 port);
    void handleVectorClock(const QVariantMap& message, const QHostAddress& addr, quint16 port);
    void sendMissingMessages(const VectorClock& peerClock, const QHostAddress& addr, quint16 port);
    VectorClock getMyVectorClock() const;
    
    // Peer management
    struct PeerInfo {
        QHostAddress address;
        quint16 port;
        QDateTime lastSeen;
        QString peerId;
    };
    
    void addPeer(const QString& peerId, const QHostAddress& addr, quint16 port);
    void updatePeerLastSeen(const QHostAddress& addr, quint16 port);
    QList<PeerInfo> getActivePeers() const;

    // UI Components
    QWidget* m_centralWidget;
    QVBoxLayout* m_mainLayout;
    QTextEdit* m_chatLog;
    QHBoxLayout* m_inputLayout;
    QComboBox* m_destinationCombo;
    QLineEdit* m_messageInput;
    QPushButton* m_sendButton;
    QPushButton* m_broadcastButton;
    QLabel* m_statusLabel;
    QLineEdit* m_peerAddressInput;
    QPushButton* m_addPeerButton;

    // Network Components
    QUdpSocket* m_udpSocket;
    
    // Timers
    QTimer* m_discoveryTimer;       // Peer discovery
    QTimer* m_antiEntropyTimer;     // Anti-entropy sync
    QTimer* m_retransmissionTimer;  // Message retransmission
    
    // Configuration
    QString m_clientId;
    int m_port;
    int m_sequenceNumber;
    
    // Message storage
    QMap<QString, QMap<int, MessageInfo>> m_messageStore; // origin -> (sequence -> MessageInfo)
    QMap<QString, QSet<int>> m_pendingAcks; // origin -> set of pending sequence numbers
    
    // Peer management
    QMap<QString, PeerInfo> m_peers; // peerId -> PeerInfo
    
    // Constants
    static const int DISCOVERY_INTERVAL = 5000;    // 5 seconds
    static const int ANTI_ENTROPY_INTERVAL = 3000; // 3 seconds
    static const int RETRANSMISSION_INTERVAL = 2000; // 2 seconds
    static const int PEER_TIMEOUT = 30000;         // 30 seconds
    static const int BASE_PORT = 9000;
    static const int MAX_PORTS = 10;
};

#endif // SIMPLECHAT_P2P_H