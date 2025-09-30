#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include "simplechat.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("SimpleChat");
    app.setApplicationVersion("1.0");
    
    QCommandLineParser parser;
    parser.setApplicationDescription("SimpleChat - Ring Network Messaging Application");
    parser.addHelpOption();
    parser.addVersionOption();
    
    // Command line options
    QCommandLineOption clientIdOption(QStringList() << "c" << "client",
                                      "Client ID (Client1, Client2, Client3, Client4)",
                                      "clientId", "Client1");
    parser.addOption(clientIdOption);
    
    QCommandLineOption listenPortOption(QStringList() << "l" << "listen",
                                        "Port to listen on",
                                        "port", "9001");
    parser.addOption(listenPortOption);
    
    QCommandLineOption targetPortOption(QStringList() << "t" << "target",
                                        "Target port (next peer in ring)",
                                        "port", "9002");
    parser.addOption(targetPortOption);
    
    parser.process(app);
    
    QString clientId = parser.value(clientIdOption);
    int listenPort = parser.value(listenPortOption).toInt();
    int targetPort = parser.value(targetPortOption).toInt();
    
    // Validate input
    if (listenPort <= 0 || targetPort <= 0) {
        qCritical() << "Invalid port numbers";
        return 1;
    }
    
    SimpleChat chat(clientId, listenPort, targetPort);
    chat.show();
    
    return app.exec();
}