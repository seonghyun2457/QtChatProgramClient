#ifndef PACKETHEADER_H
#define PACKETHEADER_H

#include <QtGlobal>

enum class ePacketType : quint8 {
    Heartbeat = 0,
    TextMessage,
    File
};

typedef struct PacketHeader {
    ePacketType packetType;
    quint32 packetSize;
    quint32 fileNameLength;
} PacketHeader_t;

#endif // PACKETHEADER_H
