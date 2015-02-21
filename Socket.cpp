// 
// 
// 

#include "Socket.h"
#include "Checksum.h"

uint8_t Socket::srcPort_L_count = 0;



void Socket::prepareIPPacket()
{
	chunk.ip.version = 4;
	chunk.ip.IHL = 0x05; //20 bytes
	chunk.ip.raw[1] = 0x00; //DSCP/ECN=0;
	chunk.ip.identification.setValue(0);
	chunk.ip.flags = 0;
	chunk.ip.fragmentOffset = 0;
	chunk.ip.checksum.setValue(0);
	chunk.ip.sourceIP = localIP;
	chunk.ip.destinationIP = remoteIP;
	chunk.ip.TTL = 255;
	chunk.ip.checksum.rawu = ~Checksum::calc(sizeof(IPHeader), (uint8_t*)&chunk.ip);
}

uint16_t Socket::calcPseudoHeaderChecksum(uint8_t protocol, uint16_t length)
{
	nint32_t pseudo;
	pseudo.h.h = 0;
	pseudo.h.l = protocol;
	pseudo.l.setValue(length);

	uint16_t sum = Checksum::calc(sizeof(IPAddress) * 2, (uint8_t*)&chunk.ip.sourceIP);
	return Checksum::calc(sum, sizeof(pseudo), (uint8_t*)&pseudo);
}


uint16_t Socket::write(uint16_t len, const byte* data)
{
	return buffer.write(len, data);
}

uint16_t Socket::write(const String& s)
{
	return buffer.write(s.length(), (uint8_t*)s.c_str());
}

uint16_t Socket::write(const __FlashStringHelper* pattern, ...)
{

	char c;
	char buf[16];
	uint8_t i = 0;
	va_list args;
	va_start(args, pattern);
	PGM_P p = (PGM_P)pattern;
	uint16_t bytes = 0;

	for (;;)
	{
		c = (char)pgm_read_byte(p++);

		if (c == '%')
		{
			c = (char)pgm_read_byte(p);
			if (c != '%')
			{
				bytes += write(i, (uint8_t*)buf);
				bytes += write(*va_arg(args, String*));
				i = 0;
				continue;
			}
			p++;
		}

		if (c == 0)
		{
			write(i, (uint8_t*)buf);
			va_end(args);
			return bytes;
		}

		buf[i] = c;
		bytes++;
		i++;
		if (i == sizeof(buf))
		{
			write(i, (uint8_t*)buf);
			i = 0;

		}


	}




}