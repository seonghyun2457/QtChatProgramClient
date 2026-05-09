#ifndef PACKETHEADER_H
#define PACKETHEADER_H

#include <QtGlobal>

constexpr quint32 NICKNAME_LENGTH = 16;
constexpr quint32 FILENAME_LENGTH = 32;

enum class ePacketType : quint8 {
    Heartbeat = 0,
    TextMessage,
    File
};

typedef struct PacketHeader {
    ePacketType packetType;
    quint32 packetSize;
    char senderNickName[NICKNAME_LENGTH + 1];
    char fileName[FILENAME_LENGTH + 1];
} PacketHeader_t;

#endif // PACKETHEADER_H
