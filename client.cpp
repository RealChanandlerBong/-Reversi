#include "client.h"
#include "const.h"
#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

Client::Client()
{
    socket = new QTcpSocket(this);
    myName = "";
    opponentName = "";
    sync = -1;
}

void Client::connectToHost(QString ip, quint16 port, QString name) {
    qDebug() << name << "is trying to connect...";
    this->myName = name;

    connect(socket, SIGNAL(connected()), this, SLOT(connected()));
    connect(socket, SIGNAL(disconnected()), this, SLOT(disconnected()));
    connect(socket, SIGNAL(readyRead()), this, SLOT(readyRead()));
    connect(socket, SIGNAL(errorOccurred(QAbstractSocket::SocketError)), this, SLOT(errorOccurred(QAbstractSocket::SocketError)));

    socket->connectToHost(ip, port);
    if (!socket->waitForConnected(3000)) {
        qDebug() << name << "connect error";
        emit clientError(socket->errorString());
        endConnection();
    }
}

void Client::connected() {
    qDebug() << myName << "is connected";
    sendMessage(CONNECT, REQUEST, myName);
}

// TODO: handle this?
void Client::disconnected() {
    qDebug() << myName << "is disconnected";
    //endConnection();
}

void Client::errorOccurred(QAbstractSocket::SocketError e) {
    Q_UNUSED(e);
    qDebug() << myName << "error:" << socket->errorString();
    emit clientError(socket->errorString());
    //endConnection();
}

void Client::endConnection() {
    socket->close();
    myName = "";
    opponentName = "";
}

void Client::readyRead() {
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

void Client::messageReceived(const QJsonObject &obj) {
    const QJsonValue type = obj.value(QLatin1String(TYPE));
    const QJsonValue content = obj.value(QLatin1String(CONTENT));
    const QJsonValue synVal = obj.value(QLatin1String(SYN));
    qDebug() << "received: {type:" << type << ", content:" << content << ", syn:" << synVal << "}";

    if (type.isNull() || !type.isString()) {
        qDebug() << "err: expecting a non-null TYPE";
        return;
    }

    // for those which only require field TYPE
    // i.e. LEAVE
    if (type.toString().compare(QLatin1String(LEAVE), Qt::CaseInsensitive) == 0) {
        qDebug() << "msg: opponent left";
        emit opponentQuit();
    }

    if (content.isNull() || !content.isString()) {
        qDebug() << "err: expecting a non-null CONTENT";
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
        return;
    }

    if (synVal.isNull()) {
        qDebug() << "err: syn field is empty";
        return;
    }

    // for those which do not require SYN to be an integer
    // i.e. CONNECT
    if (type.toString().compare(QLatin1String(CONNECT), Qt::CaseInsensitive) == 0) {
        if (!synVal.isString()) {
            qDebug() << "err: expecting a string SYN";
            return;
        }
        if (content.toString().compare(QLatin1String(ACCEPTED), Qt::CaseInsensitive) == 0) {
            opponentName = synVal.toString();
            qDebug() << "msg: " << "accepted the connection request";
            emit clientConnected();
            sendMessage(CONNECT, ACCEPTED, "0");
            //emit gameStart();
            qDebug() << "msg: game start";
        } else if (content.toString().compare(QLatin1String(REJECTED), Qt::CaseInsensitive) == 0) {
            qDebug() << "err: client connect error";
            emit clientError(synVal.toString());
            endConnection();
        }
        // otherwise is not a valid message, so just ignore it
        return;
    }

    // then, check SYN == sync + 1
    // EITHER ignore if not synchronized (will ignore cause problems?)
    // OR ask for resend?
    if (!synVal.isDouble()) {
        qDebug() << "err: syn is not a double";
        return;
    }
    if (synVal.toInt() != sync + 1) {
        // TODO: how should we handle this?
        qDebug() << "err: sync out of order; expected" << sync + 1 << ", got" << synVal.toInt();
        return;
    }

    // then, handle those which do not need field CONTENT
    // i.e. SKIP, YIELD
    if (type.toString().compare(QLatin1String(SKIP), Qt::CaseInsensitive) == 0) {
        emit opponentSkip();
        qDebug() << "msg: skip";
        return;
    } else if (type.toString().compare(QLatin1String(YIELD), Qt::CaseInsensitive) == 0) {
        emit opponentYield();
        qDebug() << "msg: yield";
        return;
    }

    // last, handle the rest
    // i.e. MOVE, REGRET

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

void Client::sendMessage(const QString &type, const QString &content, const QString &syn) {
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
        qDebug() << "sent: {type:" << type << ", content:" << content << ", syn:" <<  msg[SYN] << "}";
    }
    stream << QJsonDocument(msg).toJson();
}

const QString Client::getMyName() {
    return myName;
}

const QString Client::getOpponentName() {
    return opponentName;
}
