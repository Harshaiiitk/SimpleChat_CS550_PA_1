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
#include <QtWidgets/QListWidget>
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

// DSDV Routing Table Entry
struct RouteEntry {
    QHostAddress nextHop;  // Next hop IP address
    quint16 nextPort;      // Next hop port
    int sequenceNumber;    // DSDV sequence number
    int hopCount;          // Number of hops to destination
    QDateTime lastUpdate;  // When this route was last updated
    bool isDirect;         // Whether this is a direct route (for NAT traversal preference)
    
    // NAT traversal fields
    QHostAddress publicIP;    // Public IP discovered through NAT
    quint16 publicPort;      // Public port discovered through NAT
};

class SimpleChatP2P : public QMainWindow
{
    Q_OBJECT

public:
    SimpleChatP2P(const QString& clientId, int port, QWidget *parent = nullptr, bool noForward = false);
    ~SimpleChatP2P();

private slots:
    void sendMessage();
    void readPendingDatagrams();
    void performPeerDiscovery();
    void performAntiEntropy();
    void checkMessageRetransmission();
    void addPeerManually();
    void sendPrivateMessage();  // New: Private message handler
    void sendRouteRumor();      // New: DSDV route announcement

private:
    // UI Setup
    void setupUI();
    void setupNetwork();
    void addToMessageLog(const QString& text, const QString& sender = "");
    
    // Message handling
    void processReceivedMessage(const QVariantMap& message, const QHostAddress& senderAddr, quint16 senderPort);
    void sendMessageToPeer(const QVariantMap& message, const QHostAddress& addr, quint16 port);
    void broadcastMessage(const QVariantMap& message);
    
    // DSDV Routing
    void updateRoutingTable(const QString& destination, const QHostAddress& nextHop, quint16 nextPort, 
                          int seqNo, int hopCount, bool isDirect = false);
    void processRouteRumor(const QVariantMap& message, const QHostAddress& senderAddr, quint16 senderPort);
    void forwardPrivateMessage(const QVariantMap& message);
    bool isBetterRoute(const RouteEntry& oldRoute, const RouteEntry& newRoute);
    void updateNodeList();  // Update UI with available nodes
    
    // NAT Traversal
    void processNATInfo(const QVariantMap& message, const QHostAddress& senderAddr, quint16 senderPort);
    void addPublicEndpoint(const QString& nodeId, const QHostAddress& publicIP, quint16 publicPort);
    
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
    QPushButton* m_privateButton;   // New: Private message button
    QLabel* m_statusLabel;
    QLineEdit* m_peerAddressInput;
    QPushButton* m_addPeerButton;
    QListWidget* m_nodeListWidget;   // New: List of discovered nodes

    // Network Components
    QUdpSocket* m_udpSocket;
    
    // Timers
    QTimer* m_discoveryTimer;       // Peer discovery
    QTimer* m_antiEntropyTimer;     // Anti-entropy sync
    QTimer* m_retransmissionTimer;  // Message retransmission
    QTimer* m_routeRumorTimer;      // New: Route rumor timer for DSDV
    
    // Configuration
    QString m_clientId;
    int m_port;
    int m_sequenceNumber;
    int m_dsdvSequenceNumber;       // New: DSDV sequence number
    bool m_noForwardMode;           // New: No-forward mode for rendezvous server
    
    // Message storage
    QMap<QString, QMap<int, MessageInfo>> m_messageStore; // origin -> (sequence -> MessageInfo)
    QMap<QString, QSet<int>> m_pendingAcks; // origin -> set of pending sequence numbers
    
    // Peer management
    QMap<QString, PeerInfo> m_peers; // peerId -> PeerInfo
    
    // DSDV Routing
    QMap<QString, RouteEntry> m_routingTable; // destination -> RouteEntry
    QMap<QString, int> m_lastSeqNoSeen; // origin -> last sequence number seen
    
    // NAT Traversal
    QMap<QString, QPair<QHostAddress, quint16>> m_publicEndpoints; // nodeId -> (publicIP, publicPort)
    QSet<QString> m_natDetected; // Track which nodes we've already logged NAT detection for
    
    // Constants
    static const int DISCOVERY_INTERVAL = 5000;    // 5 seconds
    static const int ANTI_ENTROPY_INTERVAL = 3000; // 3 seconds
    static const int RETRANSMISSION_INTERVAL = 2000; // 2 seconds
    static const int ROUTE_RUMOR_INTERVAL = 60000; // 60 seconds for route rumors
    static const int PEER_TIMEOUT = 30000;         // 30 seconds
    static const int BASE_PORT = 9000;
    static const int MAX_PORTS = 10;
    static const int DEFAULT_HOP_LIMIT = 10;       // Default hop limit for private messages
};

#endif // SIMPLECHAT_P2P_H