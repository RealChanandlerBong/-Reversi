#include "const.h"
#include "server.h"
#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkInterface>

Server::Server(QObject *parent)
    : QObject{parent}
{
    server = new QTcpServer(this);
    socket = new QTcpSocket(this);
    sync = -2; // why
}

void Server::startServer(const QString &name) {
    connect(server, SIGNAL(newConnection()), this, SLOT(newConnection()));

    QHostAddress ipAddress;
    QList<QHostAddress> ipAddressesList = QNetworkInterface::allAddresses();
    // use the first non-localhost IPv4 address
    for (int i = 0; i < ipAddressesList.size(); ++i) {
        if (ipAddressesList.at(i) != QHostAddress::LocalHost &&
            ipAddressesList.at(i).toIPv4Address()) {
            ipAddress = ipAddressesList.at(i);
            break;
        }
    }
    // if we did not find one, use IPv4 localhost
    if (ipAddress.isNull()) {
        ipAddress = QHostAddress::LocalHost;
    }
    if (!server->listen(QHostAddress::Any, server->serverPort())) {
        qDebug() << "cannot start server";
        emit serverError(server->errorString());
    } else {
        qDebug() << "server is on" << ipAddress.toString() << ":" << server->serverPort();
        myName = name;
        ipAddr = ipAddress.toString();
        emit serverStarted();
    }
}

void Server::stopServer() {
    socket->close();
    server->close();
    myName = "";
    opponentName = "";
    qDebug() << "server is stopped";
    emit serverStopped();
}

// TODO: handle this
void Server::newConnection() {
    if (socket->state() == QTcpSocket::ConnectedState) {
        QTcpSocket *s = server->nextPendingConnection();
        QDataStream stream(s);
        stream.setVersion(QDataStream::Qt_6_0);

        // TODO: should we emit serverError here?
        QJsonObject msg;
        msg[TYPE] = CONNECT;
        msg[CONTENT] = REJECTED;
        msg[SYN] = "server already in use";

        stream << QJsonDocument(msg).toJson();
        s->waitForBytesWritten(3000);
        s->close();
        // should we call s->close() here?
        return;
    }
    socket = server->nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, this, &Server::readyRead);
    connect(socket, &QTcpSocket::disconnected, this, &Server::disconnected);
    connect(socket, &QTcpSocket::errorOccurred, this, &Server::errorOccurred);
    qDebug() << "new connection";
}

// TODO: need handle?
void Server::disconnected() {
    qDebug() << myName << "is disconnected";
//    emit serverError(myName + "is disconnected");
//    stopServer();
//    stop somewhere else?
}

void Server::errorOccurred(QAbstractSocket::SocketError e) {
    Q_UNUSED(e)
    qDebug() << myName << "error:" << socket->errorString();
    emit serverError(socket->errorString());
    //stopServer();
}

void Server::readyRead() {
    // prepare a container to hold the UTF-8 encoded JSON we receive from the socket
    QByteArray jsonData;
    // create a QDataStream operating on the socket
    QDataStream socketStream(socket);
    // set the version so that programs compiled with different versions of Qt can agree on how to serialise
    socketStream.setVersion(QDataStream::Qt_6_0);
    // start an infinite loop

    for (;;) {
        // we start a transaction so we can revert to the previous state in case we try to read more data than is available on the socket
        socketStream.startTransaction();
        // we try to read the JSON data
        socketStream >> jsonData;
        if (socketStream.commitTransaction()) {
            // we successfully read some data
            // we now need to make sure it's in fact a valid JSON
            QJsonParseError parseError;
            // we try to create a json document with the data we received
            const QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &parseError);
            if (parseError.error == QJsonParseError::NoError) {
                // if the data was indeed valid JSON
                if (jsonDoc.isObject()) // and is a JSON object
                    messageReceived(jsonDoc.object()); // parse the JSON
            }
            // loop and try to read more JSONs if they are available
        } else {
            // the read failed, the socket goes automatically back to the state it was in before the transaction started
            // we just exit the loop and wait for more data to become available
            break;
        }
    }
}

void Server::messageReceived(const QJsonObject &obj) {
    const QJsonValue type = obj.value(QLatin1String(TYPE));
    const QJsonValue content = obj.value(QLatin1String(CONTENT));
    const QJsonValue synVal = obj.value(QLatin1String(SYN));
    qDebug() << "received: {type:" << type << ", content:" << content << ", syn:" << synVal << "}";

    if (type.isNull() || !type.isString()) {
        qDebug() << "msg: expected a non-null TYPE";
        return;
    }

    // for those which only require field TYPE
    // i.e. LEAVE
    if (type.toString().compare(QLatin1String(LEAVE), Qt::CaseInsensitive) == 0) {
        qDebug() << "msg: opponent leave";
        emit opponentQuit();
    }

    if (content.isNull() || !content.isString()) {
        return;
    }

    // for those which do not require SYN
    // i.e. NEW
    if (type.toString().compare(QLatin1String(NEW), Qt::CaseInsensitive) == 0) {
        if (content.toString().compare(QLatin1String(ACCEPTED), Qt::CaseInsensitive) == 0) {
            qDebug() << "msg: opponent ready";
            emit opponentReady();
        } else if (content.toString().compare(QLatin1String(REJECTED), Qt::CaseInsensitive) == 0) {
            qDebug() << "msg: opponent quit";
            emit opponentQuit();
        }
    }

    if (synVal.isNull()) {
        qDebug() << "msg: syn field is empty";
        return;
    }

    // for those which do not require SYN to be an integer
    // i.e. CONNECT
    if (type.toString().compare(QLatin1String(CONNECT), Qt::CaseInsensitive) == 0) {
        if (!synVal.isString()) {
            return;
        }
        if (content.toString().compare(QLatin1String(REQUEST), Qt::CaseInsensitive) == 0) {
            opponentName = synVal.toString();
            qDebug() << "msg:" << opponentName << "request to connect";
            emit serverConnected();
            sendMessage(CONNECT, ACCEPTED, myName);
        } else if (content.toString().compare(QLatin1String(ACCEPTED), Qt::CaseInsensitive) == 0) {
            qDebug() << "msg: game start";
            emit gameStart();
        }
        return;
    }

    // then, check SYN == sync + 1
    if (!synVal.isDouble()) {
        qDebug() << "msg: syn is not a double";
        return;
    }
    if (synVal.toInt() != sync + 1) {
        qDebug() << "msg: sync out of order: expected" << sync + 1 << ", got" << synVal.toInt();
    }

    // then, handle those which do not need field CONTENT
    // i.e. SKIP, YIELD
    if (type.toString().compare(QLatin1String(SKIP), Qt::CaseInsensitive) == 0) {
        qDebug() << "msg: skip";
        emit opponentSkip();
        return;
    } else if (type.toString().compare(QLatin1String(YIELD), Qt::CaseInsensitive) == 0) {
        qDebug() << "msg: yield";
        emit opponentYield();
        return;
    }

    // last, handle the rest
    // i.e. MOVE, REGRET
    if (content.isNull() || !content.isString()) {
        return;
    }
    QStringList move = content.toString().split(SEP);
    if (type.toString().compare(QLatin1String(MOVE), Qt::CaseInsensitive) == 0) {
        if (move.length() != 2) {
            return;
        }
        bool xIsInt = false, yIsInt = false;
        uint x = move.at(0).toUInt(&xIsInt);
        uint y = move.at(1).toUInt(&yIsInt);
        if (!xIsInt || !yIsInt || x >= 8 || y >= 8) {
            return;
        }
        qDebug() << "msg: move (" << x << "," << y << ")";
        emit setCellAt(x, y);
    } else if (type.toString().compare(QLatin1String(REGRET), Qt::CaseInsensitive) == 0) {
        if (move.length() == 1) {
            if (move.at(0).compare(QLatin1String(ACCEPTED), Qt::CaseInsensitive) == 0) {
                qDebug() << "msg: regret accepted";
                emit regretRequestAccepted();
            } else if (move.at(0).compare(QLatin1String(REJECTED), Qt::CaseInsensitive) == 0) {
                qDebug() << "msg: regret rejected";
                emit regretRequestRejected();
            }
        } else if (move.length() == 0) {
            qDebug() << "msg: regret requested";
            emit opponentRequestRegret();
        }
    }
}

void Server::sendMessage(const QString &type, const QString &content, const QString &syn) {
    QDataStream stream(socket);
    stream.setVersion(QDataStream::Qt_6_0);

    QJsonObject msg;
    msg[TYPE] = type;
    msg[CONTENT] = content;
    if (syn.isEmpty()) {
        sync += 2;
        msg[SYN] = sync;
        qDebug() << "sent: {type:" << type << ", content:" << content << ", sync:" << msg[SYN] << "}";
    } else {
        msg[SYN] = syn;
        qDebug() << "sent: {type:" << type << ", content:" << content << ", syn:" << msg[SYN] << "}";
    }
    stream << QJsonDocument(msg).toJson();
}

const QString Server::getIpAddress() {
    return ipAddr;
}

quint16 Server::getPort() {
    return server->serverPort();
}

const QString Server::getMyName() {
    return myName;
}

const QString Server::getOpponentName() {
    return opponentName;
}
