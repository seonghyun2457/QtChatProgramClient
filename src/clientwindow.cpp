#include "clientwindow.h"
#include "ui_clientwindow.h"

#include <QDebug>

ClientWindow::ClientWindow(QWidget *parent)
    : QWidget(parent)
    , mIPv4Address("")
    , mPort("")
    , mNickname("")
    , mMessage("")
    , mStatusLog("")
    , mUi(std::make_unique<Ui::ClientWindow>())
{
    // Reserve size
    mMessage.reserve(256);
    mStatusLog.reserve(256);


    // CONNECT
    connect(&mSocket, &QTcpSocket::connected, this, &ClientWindow::connected);
    connect(&mSocket, &QTcpSocket::disconnected, this, &ClientWindow::disconnected);
    connect(&mSocket, &QTcpSocket::stateChanged, this, &ClientWindow::stateChanged);
    connect(&mSocket, &QTcpSocket::readyRead, this, &ClientWindow::readyRead, Qt::QueuedConnection);
    connect(&mSocket, &QTcpSocket::errorOccurred, this, &ClientWindow::error);

    // Set GUI
    mUi->setupUi(this);
}

ClientWindow::~ClientWindow()
{
    if (mSocket.isOpen()) {
        mSocket.disconnectFromHost();
        mSocket.close();
    }
}

void ClientWindow::send(const QString &iMessage)
{
    if (mSocket.state() != QAbstractSocket::ConnectedState) {
        mStatusLog = "Failed to send a message due to disconnection.";
        return;
    }
    QString aMessage = mNickname + ": " + iMessage;
    mUi->teMessageLog->append(aMessage);
    mSocket.write(aMessage.toUtf8());
}

void ClientWindow::clearAllMessages()
{
    mUi->teMessageLog->clear();
    mUi->leMessage->clear();
}

void ClientWindow::connected()
{
    qDebug() << "connected";

    mUi->leIPv4->setReadOnly(true);
    mUi->lePort->setReadOnly(true);
    mUi->leNickname->setReadOnly(true);

    mUi->leMessage->setReadOnly(false);
}

void ClientWindow::disconnected()
{
    qDebug() << "disconnected";

    mUi->leIPv4->setReadOnly(false);
    mUi->lePort->setReadOnly(false);
    mUi->leNickname->setReadOnly(false);

    mUi->leMessage->setReadOnly(true);
    mUi->leMessage->clear();
}

void ClientWindow::stateChanged()
{
    qDebug() << "stateChanged";
    qDebug() << "mSocket.state(): " << mSocket.state();
}

void ClientWindow::readyRead()
{
    QByteArray receivedMessage = mSocket.readAll();
    QString message = QString::fromUtf8(receivedMessage);
    qDebug() << "message: " << message;

    if (message == "Server: heartbeat ping\n") {
        mSocket.write("Client: heartbeat pong\n");
        return;
    }

    mUi->teMessageLog->append(message);
}

void ClientWindow::error(QAbstractSocket::SocketError socketError)
{
    mStatusLog = "Socket closed. Error code " + QString::number(socketError) + ": " + mSocket.errorString();
    mUi->lbStatus->setText(mStatusLog);

    mSocket.close();
}

void ClientWindow::on_leIPv4_textChanged(const QString &arg1)
{
    mIPv4Address = arg1;
}

void ClientWindow::on_leNickname_textChanged(const QString &arg1)
{
    mNickname = arg1;
}

void ClientWindow::on_lePort_textChanged(const QString &arg1)
{
    mPort = arg1;
}

void ClientWindow::on_btnConnect_clicked()
{
    mStatusLog = "";
    if (mSocket.isOpen()) {
        mStatusLog = "socket is already connected to the server:\nServer IPv4 in use: " + mSocket.peerAddress().toString() + ", Server Port in use: " + QString::number(mSocket.peerPort())
                + "\n\nClient socket:\nIPv4: " + mSocket.localAddress().toString() + "\nPort: " + QString::number(mSocket.localPort());
        mUi->lbStatus->setText(mStatusLog);
        return;
    }

    const QHostAddress hostAddress(mIPv4Address);
    if (hostAddress.isNull()) {
        mStatusLog = "Server address format is invalid.";
        mUi->lbStatus->setText(mStatusLog);
        return;
    }

    bool ok = false;
    quint16 port = mPort.toUInt(&ok);
    if (!ok) {
        mStatusLog = "Input an integer number in Port.";
        mUi->lbStatus->setText(mStatusLog);
        return;
    }

    if (mNickname == "") {
        mStatusLog = "Input your nickname.";
        mUi->lbStatus->setText(mStatusLog);
        return;
    }

    mSocket.connectToHost(hostAddress, port);

    if (!mSocket.isOpen()) {
        mUi->lbStatus->setText(mSocket.errorString());
        return;
    }

    if (!mSocket.waitForConnected()) {
        mUi->lbStatus->setText(mSocket.errorString());
        return;
    }

    clearAllMessages();

    mStatusLog = "Connected to the server:\nServer IPv4: " + mSocket.peerAddress().toString() + ", Server Port: " + QString::number(mSocket.peerPort())
            + "\n\nClient socket:\nIPv4: " + mSocket.localAddress().toString() + ", Port: " + QString::number(mSocket.localPort());
    mUi->lbStatus->setText(mStatusLog);

    QString aMessage = mNickname + " joined this chatroom.";
    mUi->teMessageLog->append(aMessage);
    mSocket.write(aMessage.toUtf8());
}

void ClientWindow::on_btnStop_clicked()
{
    mStatusLog = "";

    if (!mSocket.isOpen()) {
        mStatusLog = "Socket is already closed.";
        mUi->lbStatus->setText(mStatusLog);
        return;
    }

    if (mSocket.state() == QAbstractSocket::ConnectedState) {
        QString aMessage = mNickname + " left this chatroom.";
        mUi->teMessageLog->append(aMessage);
        mSocket.write(aMessage.toUtf8());
    }

    mSocket.disconnectFromHost();
    mSocket.close();

    mStatusLog = "Socket closed.";
    mUi->lbStatus->setText(mStatusLog);
}

void ClientWindow::on_btnQuit_clicked()
{
    if (mSocket.isOpen()) {
        mSocket.disconnectFromHost();
        mSocket.close();
    }

    close();
}

void ClientWindow::on_btnSend_clicked()
{
    on_leMessage_editingFinished();
}

void ClientWindow::on_leMessage_editingFinished()
{
    send(mMessage);
    mUi->leMessage->clear();
}

void ClientWindow::on_leMessage_textChanged(const QString &arg1)
{
    mMessage = arg1;
}

