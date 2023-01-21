#ifndef CLIENT_H
#define CLIENT_H

#include <QDebug>
#include <QObject>
#include <QDataStream>
#include <QTcpSocket>

class Client: public QObject
{
    Q_OBJECT
public:
    Client();
    void connectToHost(QString ip, quint16 port, QString name);
    void sendMessage(const QString &type, const QString &content, const QString &syn);
    void endConnection();
    const QString getMyName();
    const QString getOpponentName();

signals:
    void clientConnected();
    void clientError(const QString &err);
    void gameStart();
    void setCellAt(uint x, uint y);
    void opponentReady();
    void opponentQuit();
    void opponentSkip();
    void opponentYield();
    void opponentRequestRegret();
    void regretRequestAccepted();
    void regretRequestRejected();

private slots:
    void connected();
    void disconnected();
    void readyRead();
    void errorOccurred(QAbstractSocket::SocketError e);

private:
    void messageReceived(const QJsonObject &obj);

    QTcpSocket *socket;
    QString myName, opponentName;
    int sync;
};

#endif // CLIENT_H
