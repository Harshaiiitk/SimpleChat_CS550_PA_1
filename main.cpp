#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include "simplechatp2p.h"
#include <QtNetwork/QUdpSocket>
#include <QVariantMap>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("SimpleChatP2P");
    app.setApplicationVersion("3.0");  // Updated version for DSDV

    QCommandLineParser parser;
    parser.setApplicationDescription("SimpleChat - UDP P2P/Broadcast Messaging with DSDV Routing");
    parser.addHelpOption();
    parser.addVersionOption();

    // Command line options
    QCommandLineOption clientIdOption(QStringList() << "c" << "client",
                                      "Client ID (e.g., Client1)",
                                      "clientId", "Client1");
    parser.addOption(clientIdOption);

    QCommandLineOption portOption(QStringList() << "p" << "port",
                                  "UDP port to listen on",
                                  "port", "9001");
    parser.addOption(portOption);

    QCommandLineOption peerOption(QStringList() << "P" << "peer",
                                  "Optional peer to contact at startup (IP:Port). May be repeated.",
                                  "ip:port");
    parser.addOption(peerOption);

    // Add no-forward option for rendezvous server mode
    QCommandLineOption noForwardOption(QStringList() << "n" << "noforward",
                                       "No-forward mode (rendezvous server)");
    parser.addOption(noForwardOption);
    
    // Add connect option for easier NAT traversal testing
    QCommandLineOption connectOption(QStringList() << "C" << "connect",
                                     "Connect to rendezvous server at this port on localhost",
                                     "port");
    parser.addOption(connectOption);

    parser.process(app);

    const QString clientId = parser.value(clientIdOption);
    bool ok = false;
    int listenPort = parser.value(portOption).toInt(&ok);
    if (!ok || listenPort <= 0 || listenPort > 65535) {
        qCritical() << "Invalid --port value";
        return 1;
    }
    
    bool noForwardMode = parser.isSet(noForwardOption);

    SimpleChatP2P window(clientId, listenPort, nullptr, noForwardMode);
    window.show();

    // Handle connect option for NAT traversal testing
    if (parser.isSet(connectOption)) {
        bool connectOk = false;
        int rendezvousPort = parser.value(connectOption).toInt(&connectOk);
        if (connectOk && rendezvousPort > 0 && rendezvousPort <= 65535) {
            // Send initial discovery to rendezvous server
            QUdpSocket udp;
            QVariantMap discovery;
            discovery["Type"] = "discovery";
            discovery["Origin"] = clientId;
            discovery["Port"] = listenPort;

            // Serialize similarly to SimpleChatP2P::serializeMessage
            QByteArray payload;
            {
                QByteArray data;
                QDataStream stream(&data, QIODevice::WriteOnly);
                stream.setVersion(QDataStream::Qt_6_0);
                stream << quint32(0xCAFEBABE);
                stream << discovery;
                QDataStream sizeStream(&payload, QIODevice::WriteOnly);
                sizeStream.setVersion(QDataStream::Qt_6_0);
                sizeStream << quint32(data.size());
                payload.append(data);
            }
            
            udp.writeDatagram(payload, QHostAddress::LocalHost, rendezvousPort);
            qDebug() << "Sent discovery to rendezvous server at port" << rendezvousPort;
        }
    }

    // Prime with optional peers to accelerate discovery
    const QStringList peers = parser.values(peerOption);
    if (!peers.isEmpty()) {
        QUdpSocket udp;
        QVariantMap discovery;
        discovery["Type"] = "discovery";
        discovery["Origin"] = clientId;
        discovery["Port"] = listenPort;

        // Serialize similarly to SimpleChatP2P::serializeMessage
        QByteArray payload;
        {
            QByteArray data;
            QDataStream stream(&data, QIODevice::WriteOnly);
            stream.setVersion(QDataStream::Qt_6_0);
            stream << quint32(0xCAFEBABE);
            stream << discovery;
            QDataStream sizeStream(&payload, QIODevice::WriteOnly);
            sizeStream.setVersion(QDataStream::Qt_6_0);
            sizeStream << quint32(data.size());
            payload.append(data);
        }

        for (const QString &peer : peers) {
            const QStringList parts = peer.split(":");
            if (parts.size() != 2) continue;
            QHostAddress addr(parts[0]);
            bool okPort = false;
            quint16 p = parts[1].toUShort(&okPort);
            if (!addr.isNull() && okPort) {
                udp.writeDatagram(payload, addr, p);
            }
        }
    }

    return app.exec();
}