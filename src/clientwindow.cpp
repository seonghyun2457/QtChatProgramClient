#include "clientwindow.h"
#include "ui_clientwindow.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>

ClientWindow::ClientWindow(QWidget *parent)
    : QWidget(parent)
    , mIPv4Address("")
    , mPort("")
    , mNickname("")
    , mMessage("")
    , mStatusLog("")
    , mAttachedFileName("")
    , mUi(std::make_unique<Ui::ClientWindow>())
{
    memset(&mHeader, 0, sizeof(PacketHeader));

    // Reserve size
    mMessage.reserve(256);
    mStatusLog.reserve(256);
    mAttachedFileName.reserve(256);


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

void ClientWindow::send()
{
    if (mSocket.state() != QAbstractSocket::ConnectedState) {
        mStatusLog = "Failed to send due to disconnection.";
        return;
    }

    if (mAttachedFileName != "") {
        QFile attachedFile(mAttachedFileName);

        if (attachedFile.open(QIODevice::ReadOnly)) {
            // const qint64 attachedFileSize = attachedFile.size();
            QByteArray byteFile = attachedFile.readAll();
            writePacket(ePacketType::File, byteFile);
        } else {
            qCritical() << "Couldn't open " << mAttachedFileName << ".";
        }

        mAttachedFileName = "";
    }

    if (mMessage == "") return;

    QString message = mNickname + ": " + mMessage;

    writePacket(ePacketType::TextMessage, message.toUtf8());

    mUi->teMessageLog->append(message);
}

void ClientWindow::clearAllMessages()
{
    mUi->teMessageLog->clear();
    mUi->leMessage->clear();
}

void ClientWindow::writePacket(const ePacketType iPacketType, const QByteArray& iPayload)
{
    PacketHeader packetHeader;
    packetHeader.packetType = iPacketType;
    packetHeader.packetSize = iPayload.size();

    switch (iPacketType) {
    case ePacketType::Heartbeat:
        break;
    case ePacketType::TextMessage:
        break;
    case ePacketType::File:
        break;
    default:
        qCritical() << "Invalid packet type.";
        return;
    }

    // packet = header + payload
    QByteArray packet;
    packet.reserve(sizeof(PacketHeader) + packetHeader.packetSize);

    packet.append((char*)&packetHeader, sizeof(PacketHeader));
    packet.append(iPayload);

    mSocket.write(packet);
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

    // Push message to buffer
    mBuffer.append(mSocket.readAll());

    // Parse message
    while (true) {
        // Read header
        PacketHeader header;
        const char* pData = mBuffer.data();
        memcpy(&header, pData, sizeof(PacketHeader));

        // If we don't know message size
        if (header.packetSize == 0) {
            if (mBuffer.size() < sizeof(PacketHeader)) {
                break;
            }
        }

        // We know message size but the whole message isn't given yet
        if (mBuffer.size() < header.packetSize + sizeof(PacketHeader)) {
            break;
        }

        // Whole message is given
        QByteArray receivedData = mBuffer.sliced(sizeof(PacketHeader), header.packetSize);
        mBuffer.clear();

        qDebug() << "Parsed complete message from " << mSocket.peerAddress().toString() << ":" << receivedData;

        // Heartbeat
        if (header.packetType == ePacketType::Heartbeat) {
            writePacket(ePacketType::Heartbeat, "Client: heartbeat pong\n");
            break;
        } else if (header.packetType == ePacketType::File) {
            QString filePath = QDir(QDir::currentPath()).filePath("output.txt");
            qDebug() << "filePath: " << filePath;
            QFile receivedFile(filePath);

            if (receivedFile.open(QIODevice::WriteOnly)) {
                receivedFile.write(receivedData);
                receivedFile.close();
            } else {
                qCritical() << "Couldn't open receivedFile.";
            }

            break;
        }

        mUi->teMessageLog->append(receivedData);
    }
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

    if (mSocket.state() != QAbstractSocket::ConnectedState) {

        return;
    }

    clearAllMessages();

    mStatusLog = "Connected to the server:\nServer IPv4: " + mSocket.peerAddress().toString() + ", Server Port: " + QString::number(mSocket.peerPort())
            + "\n\nClient socket:\nIPv4: " + mSocket.localAddress().toString() + ", Port: " + QString::number(mSocket.localPort());
    mUi->lbStatus->setText(mStatusLog);

    QString aMessage = mNickname + " joined this chatroom.";
    writePacket(ePacketType::TextMessage, aMessage.toUtf8());

    mUi->teMessageLog->append(aMessage);
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
        writePacket(ePacketType::TextMessage, aMessage.toUtf8());

        mUi->teMessageLog->append(aMessage);
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

void ClientWindow::on_btnAttach_clicked()
{
    qDebug() << "on_btnAttach_clicked";
    mAttachedFileName = QFileDialog::getOpenFileName(this, "Select a file", QDir::homePath());
    qDebug() << "fileName: " << mAttachedFileName;
}

void ClientWindow::on_leMessage_editingFinished()
{
    send();
    mUi->leMessage->clear();
}

void ClientWindow::on_leMessage_textChanged(const QString &arg1)
{
    mMessage = arg1;
}

