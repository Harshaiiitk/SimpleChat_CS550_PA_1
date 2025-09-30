#ifndef SIMPLECHAT_H
#define SIMPLECHAT_H

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QTcpServer>
#include <QDataStream>
#include <QVariantMap>
#include <QTimer>
#include <queue>

class SimpleChat : public QMainWindow
{
    Q_OBJECT

public:
    SimpleChat(const QString& clientId, int listenPort, int targetPort, QWidget *parent = nullptr);
    ~SimpleChat();

private slots:
    void sendMessage();
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();
    void connectToNext();
    void processMessageQueue();

private:
    void setupUI();
    void setupNetwork();
    void sendMessageToRing(const QVariantMap& message);
    void processReceivedMessage(const QVariantMap& message);
    void addToMessageLog(const QString& text, const QString& sender = "");
    QByteArray serializeMessage(const QVariantMap& message);
    QVariantMap deserializeMessage(const QByteArray& data);

    // UI Components
    QWidget* m_centralWidget;
    QVBoxLayout* m_mainLayout;
    QTextEdit* m_chatLog;
    QHBoxLayout* m_inputLayout;
    QComboBox* m_destinationCombo;
    QLineEdit* m_messageInput;
    QPushButton* m_sendButton;
    QLabel* m_statusLabel;

    // Network Components
    QTcpServer* m_server;
    QTcpSocket* m_nextPeerSocket;
    QList<QTcpSocket*> m_incomingConnections;
    
    // Message Management
    std::queue<QVariantMap> m_messageQueue;
    QTimer* m_queueTimer;

    // Configuration
    QString m_clientId;
    int m_listenPort;
    int m_targetPort;
    int m_sequenceNumber;
    
    // Static peer configuration
    static const QStringList s_peerIds;
    static const QList<int> s_peerPorts;
};

#endif // SIMPLECHAT_H