#include "clientwindow.h"
#include "ui_clientwindow.h"

#include "filesenderdialog.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>

#include <QDesktopServices>
#include <QUrl>

const QString ClientWindow::s_joiningMessage = "joined this chatroom.";
const QString ClientWindow::s_leavingMessage = "left this chatroom.";

ClientWindow::ClientWindow(QWidget *parent)
    : QWidget(parent)
    , mIPv4Address("")
    , mPort("")
    , mNickname("")
    , mMessage("")
    , mStatusLog("")
    , mAttachedFilePath("")
    , mUi(std::make_unique<Ui::ClientWindow>())
{
    memset(&mHeader, 0, sizeof(PacketHeader));

    // Reserve size
    mMessage.reserve(256);
    mStatusLog.reserve(256);
    mAttachedFilePath.reserve(256);


    // CONNECT
    connect(&mSocket, &QTcpSocket::connected, this, &ClientWindow::connected);
    connect(&mSocket, &QTcpSocket::disconnected, this, &ClientWindow::disconnected);
    connect(&mSocket, &QTcpSocket::stateChanged, this, &ClientWindow::stateChanged);
    connect(&mSocket, &QTcpSocket::readyRead, this, &ClientWindow::readyRead, Qt::QueuedConnection);
    connect(&mSocket, &QTcpSocket::errorOccurred, this, &ClientWindow::error);

    // Set GUI
    mUi->setupUi(this);

    mUi->tbMessageLog->setOpenLinks(false);
    // Open a new window when clicking a link
    connect(mUi->tbMessageLog, &QTextBrowser::anchorClicked, this, [](const QUrl &url){QDesktopServices::openUrl(url);});
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

    if (mMessage == "") return;

    PacketHeader header;
    header.packetType = ePacketType::TextMessage;
    header.packetSize = mMessage.size();
    qstrncpy(header.senderNickName, mNickname.toUtf8().constData(), sizeof(header.senderNickName));
    memset(header.fileName, 0, sizeof(header.fileName));

    writePacket(header, mMessage.toUtf8());
    mUi->tbMessageLog->append(QString("<span>%1: %2</span>").arg(mNickname).arg(mMessage.toHtmlEscaped()));

    qDebug() << "mMessage: " << mMessage;
}

void ClientWindow::sendFile()
{
    if (mSocket.state() != QAbstractSocket::ConnectedState) {
        mStatusLog = "Failed to send due to disconnection.";
        return;
    }

    if (mAttachedFilePath != "") {
        QFileInfo fileInfo(mAttachedFilePath);
        const QString fileName = fileInfo.fileName();
        QByteArray fileNameByte = fileName.toUtf8();

        QFile attachedFile(mAttachedFilePath);

        if (attachedFile.open(QIODevice::ReadOnly)) {
            // Send file
            QByteArray dataByte = attachedFile.readAll();
            attachedFile.close();

            PacketHeader header;
            header.packetType = ePacketType::File;
            header.packetSize = dataByte.size();
            qstrncpy(header.senderNickName, mNickname.toUtf8().constData(), sizeof(header.senderNickName));
            qstrncpy(header.fileName, fileName.toUtf8().constData(), sizeof(header.fileName));

            writePacket(header, dataByte);

            QString link = QString("<span>%1 sent <a href=\"file:///%2\">%3</a></span>").arg(mNickname).arg(mAttachedFilePath).arg(fileName);
            mUi->tbMessageLog->append(link);
        } else {
            qCritical() << "Couldn't open " << mAttachedFilePath << ".";
        }

        mAttachedFilePath = "";
    }
}

void ClientWindow::clearAllMessages()
{
    mUi->tbMessageLog->clear();
    mUi->leMessage->clear();
}

void ClientWindow::writePacket(const PacketHeader iPacketHeader, const QByteArray& iPayload)
{
    // packet = header + payload
    QByteArray packet;
    packet.reserve(sizeof(PacketHeader) + iPacketHeader.packetSize);

    packet.append((char*)&iPacketHeader, sizeof(PacketHeader));
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
    qDebug() << "mBuffer: " << mBuffer;

    // Parse message
    // Read header
    PacketHeader header;
    const char* pData = mBuffer.data();
    memcpy(&header, pData, sizeof(PacketHeader));

    QString senderNickname(header.senderNickName);
    QString fileName(header.fileName);

    // Whole message is given
    QByteArray receivedData = mBuffer.sliced(sizeof(PacketHeader), header.packetSize);
    mBuffer.remove(0, sizeof(PacketHeader) + header.packetSize);


    // Heartbeat
    if (header.packetType == ePacketType::Heartbeat) {
        static const QString heartBeatResponse = "Client: heartbeat pong\n";
        QByteArray heartBeatResponseByte = heartBeatResponse.toUtf8();

        PacketHeader heartBeatHeader;
        heartBeatHeader.packetType = ePacketType::Heartbeat;
        heartBeatHeader.packetSize = heartBeatResponseByte.size();
        qstrncpy(heartBeatHeader.senderNickName, mNickname.toUtf8().constData(), sizeof(heartBeatHeader.senderNickName));
        memset(heartBeatHeader.fileName, 0, sizeof(heartBeatHeader.fileName));

        writePacket(heartBeatHeader, heartBeatResponseByte);
    } else if (header.packetType == ePacketType::File) {
        qDebug() << "fileName: " << fileName;
        qDebug() << "receivedData: " << receivedData;
        QString filePath = QDir(QDir::currentPath()).filePath(fileName);
        qDebug() << "filePath: " << filePath;


        QFile receivedFile(filePath);

        if (receivedFile.open(QIODevice::WriteOnly)) {
            receivedFile.write(receivedData);
            receivedFile.close();

            QString link = QString("<span>%1 sent <a href=\"file:///%2\">%3</a></span>").arg(mNickname).arg(filePath).arg(fileName);
            mUi->tbMessageLog->append(link);
        } else {
            qCritical() << "Couldn't open receivedFile.";
        }

    } else {
        mUi->tbMessageLog->append(QString("<span>%1: %2</span>").arg(senderNickname).arg(QString::fromUtf8(receivedData).toHtmlEscaped()));
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


    PacketHeader header;
    header.packetType = ePacketType::TextMessage;
    header.packetSize = s_joiningMessage.size();
    qstrncpy(header.senderNickName, mNickname.toUtf8().constData(), sizeof(header.senderNickName));
    memset(header.fileName, 0, sizeof(header.fileName));

    writePacket(header, s_joiningMessage.toUtf8());

    mUi->tbMessageLog->append(QString("<span>%1: %2</span>").arg(mNickname).arg(s_joiningMessage));
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
        PacketHeader header;
        header.packetType = ePacketType::TextMessage;
        header.packetSize = s_leavingMessage.size();
        qstrncpy(header.senderNickName, mNickname.toUtf8().constData(), sizeof(header.senderNickName));
        memset(header.fileName, 0, sizeof(header.fileName));

        writePacket(header, s_leavingMessage.toUtf8());

        mUi->tbMessageLog->append(QString("<span>%1: %2</span>").arg(mNickname).arg(s_leavingMessage));
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

void ClientWindow::on_btnFile_clicked()
{
    qDebug() << "on_btnFile_clicked";
    mAttachedFilePath = QFileDialog::getOpenFileName(this, "Select a file", QDir::homePath());
    qDebug() << "filePath: " << mAttachedFilePath;

    if (mAttachedFilePath != "") {
        QFileInfo fileInfo(mAttachedFilePath);
        const QString fileName = fileInfo.fileName();
        const qint64 fileSize = fileInfo.size();

        // Convert fileSize unit
        QString sizeStr;
        if (fileSize < 1024) sizeStr = QString::number(fileSize) + "B";
        else if (fileSize < 1024 * 1024) sizeStr = QString::number(fileSize / 1024.0, 'f', 1) + "KB";
        else sizeStr = QString::number(fileSize / (1024.0 * 1024.0), 'f', 1) + "MB";

        qDebug() << "fileName: " << fileName;
        qDebug() << "sizeStr: " << sizeStr;

        FileSenderDialog fileSenderDialog(fileName, this);

        if (fileSenderDialog.exec() == QDialog::Accepted) {
            sendFile();
        }

        mAttachedFilePath = "";
    }
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

