#ifndef CLIENTWINDOW_H
#define CLIENTWINDOW_H

#include <QWidget>
#include <QTcpSocket>


QT_BEGIN_NAMESPACE
namespace Ui {
class ClientWindow;
}
QT_END_NAMESPACE

class ClientWindow : public QWidget
{
    Q_OBJECT

public:
    ClientWindow(QWidget *parent = nullptr);
    virtual ~ClientWindow();

private slots:
    void connected();
    void disconnected();
    void stateChanged();
    void readyRead();
    void error(QAbstractSocket::SocketError socketError);

    // IP address and  Port
    void on_lePort_textChanged(const QString &arg1);
    void on_leIPv4_textChanged(const QString &arg1);
    void on_leNickname_textChanged(const QString &arg1);

    // Buttons
    void on_btnConnect_clicked();
    void on_btnStop_clicked();
    void on_btnQuit_clicked();
    void on_btnSend_clicked();

    // Message
    void on_leMessage_editingFinished();
    void on_leMessage_textChanged(const QString &arg1);


private:
    void send(const QString& iMessage);
    void clearAllMessages();
    void writePacket(const QByteArray& iMessage);

private:
    // TCP socket
    QTcpSocket mSocket;

    QString mIPv4Address;
    QString mPort;
    QString mNickname;

    // Message
    QString mMessage;

    // Status Log
    QString mStatusLog;

    // Message Parsing
    QByteArray mBuffer;
    quint32 mExpectedMessageSize;

    std::unique_ptr<Ui::ClientWindow> mUi;
};
#endif // CLIENTWINDOW_H
