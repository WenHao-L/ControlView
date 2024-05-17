#ifndef CRC16_H
#define CRC16_H

#include <QByteArray>

class CRC16
{
public:
    static uint16_t calculate(const QByteArray &data);
};

#endif // CRC16_H
