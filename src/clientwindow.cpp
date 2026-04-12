#include "clientwindow.h"
#include "ui_clientwindow.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>

#include <QDesktopServices>
#include <QUrl>

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

    QString message = mNickname + ": " + mMessage;
    QByteArray messageByte = message.toUtf8();
    PacketHeader header;
    header.packetType = ePacketType::TextMessage;
    header.packetSize = messageByte.size();
    header.fileNameLength = 0;

    writePacket(header, messageByte);

    mUi->tbMessageLog->append(message);
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
            dataByte.prepend(fileNameByte);

            PacketHeader header;
            header.packetType = ePacketType::File;
            header.packetSize = dataByte.size();
            header.fileNameLength = fileNameByte.size();

            writePacket(header, dataByte);

            memset(&header, 0, sizeof(PacketHeader));

            // send a log
            QString message = mNickname + " sent " + fileName + ".";
            QByteArray messageByte = message.toUtf8();

            header.packetType = ePacketType::TextMessage;
            header.packetSize = messageByte.size();
            header.fileNameLength = 0;

            writePacket(header, messageByte);

            mUi->tbMessageLog->append(message);
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
        mBuffer.remove(0, sizeof(PacketHeader) + header.packetSize);

        qDebug() << "Parsed complete message from " << mSocket.peerAddress().toString() << ":" << receivedData;

        // Heartbeat
        if (header.packetType == ePacketType::Heartbeat) {
            static const QString heartBeatResponse = "Client: heartbeat pong\n";
            QByteArray heartBeatResponseByte = heartBeatResponse.toUtf8();

            PacketHeader heartBeatHeader;
            heartBeatHeader.packetType = ePacketType::Heartbeat;
            heartBeatHeader.packetSize = heartBeatResponseByte.size();
            heartBeatHeader.fileNameLength = 0;

            writePacket(heartBeatHeader, heartBeatResponseByte);

            continue;

        } else if (header.packetType == ePacketType::File) {
            QByteArray fileName = receivedData.sliced(0, header.fileNameLength);
            qDebug() << "fileName: " << fileName;
            QByteArray fileData = receivedData.sliced(header.fileNameLength, header.packetSize - header.fileNameLength);
            qDebug() << "fileData: " << fileData;
            QString filePath = QDir(QDir::currentPath()).filePath(fileName);
            qDebug() << "filePath: " << filePath;


            QFile receivedFile(filePath);

            if (receivedFile.open(QIODevice::WriteOnly)) {
                receivedFile.write(fileData);
                receivedFile.close();

                QTextBrowser *browser = new QTextBrowser(this);
                browser->setReadOnly(true);

                QString link = QString("<a href=\"file:///%1\">Open file: %2</a>").arg(filePath).arg(fileName);
                browser->setHtml(link);

                connect(browser, &QTextBrowser::anchorClicked, [](const QUrl &url) {
                    if (url.isLocalFile()) {
                        QDesktopServices::openUrl(url);
                    }
                });
            } else {
                qCritical() << "Couldn't open receivedFile.";
            }

            continue;
        }

        mUi->tbMessageLog->append(receivedData);
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

    QString message = mNickname + " joined this chatroom.";
    QByteArray messageByte = message.toUtf8();

    PacketHeader header;
    header.packetType = ePacketType::TextMessage;
    header.packetSize = messageByte.size();
    header.fileNameLength = 0;

    writePacket(header, messageByte);

    mUi->tbMessageLog->append(message);
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
        QString message = mNickname + " left this chatroom.";
        QByteArray messageByte = message.toUtf8();

        PacketHeader header;
        header.packetType = ePacketType::TextMessage;
        header.packetSize = messageByte.size();
        header.fileNameLength = 0;

        writePacket(header, messageByte);

        mUi->tbMessageLog->append(message);
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

        sendFile();

        /*
        // UI 구성
        QHBoxLayout layout = QHBoxLayout(this);
        auto *nameLabel = new QLabel(fileName, this);
        auto *sizeLabel = new QLabel(sizeStr, this);
        auto *removeBtn = new QPushButton("X", this);

        nameLabel->setStyleSheet("font-weight: bold;");
        sizeLabel->setStyleSheet("color: gray;");
        removeBtn->setFixedSize(20, 20);
        this->setStyleSheet("background-color: #f0f0f0; border-radius: 5px;");

        layout.addWidget(nameLabel);
        layout.addWidget(sizeLabel);
        layout.addStretch();
        layout.addWidget(removeBtn);

        // 삭제 버튼 클릭 시 위젯 제거
        connect(removeBtn, &QPushButton::clicked, this, &QHBoxLayout::deleteLater);
        */
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

