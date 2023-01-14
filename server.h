#ifndef SERVER_H
#define SERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

class Server : public QObject
{
    Q_OBJECT
public:
    explicit Server(QObject *parent = nullptr);
    void startServer(const QString &name);
    void stopServer();
    void sendMessage(const QString &type, const QString &content, const QString &syn);

    const QString getMyName();
    const QString getOpponentName();
    const QString getIpAddress();
    quint16 getPort();

signals:
    void serverStarted();
    void serverConnected();
    void serverStopped(); // not handled // necessary?
    void serverError(const QString &msg);
    void opponentReady();
    void opponentQuit();
    void gameStart();
    void setCellAt(uint x, uint y);
    void opponentSkip();
    void opponentYield();
    void opponentRequestRegret();
    void regretRequestAccepted();
    void regretRequestRejected();

private slots:
    void newConnection();
    void readyRead();
    void disconnected();
    void errorOccurred(QAbstractSocket::SocketError e);

private:
    void messageReceived(const QJsonObject &obj);

    QTcpServer *server;
    QTcpSocket *socket;
    QString myName, opponentName;
    QString ipAddr;
    int sync;
};

#endif // SERVER_H
