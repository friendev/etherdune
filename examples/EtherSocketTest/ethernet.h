
// PIN Connections (Using Arduino UNO):
//   VCC -   3.3V
//   GND -    GND
//   SCK - Pin 13
//   SO  - Pin 12
//   SI  - Pin 11
//   CS  - Pin  8
//
#define __PROG_TYPES_COMPAT__

#ifndef ESEthernet_h
#define ESEthernet_h


#if ARDUINO >= 100
#include <Arduino.h> // Arduino 1.0
#else
#include <WProgram.h> // Arduino 0022
#endif

#include <avr/pgmspace.h>
#include "net.h"
#include "inet.h"
#include "enc28j60constants.h"
#include "config.h"
#include "Socket.h"


class Socket;



class EtherSocket
{
	friend class Socket;
public:
	static MACAddress localMAC;
	static IPAddress localIP;
	static bool broadcast_enabled; //!< True if broadcasts enabled (used to allow temporary disable of broadcast for DHCP or other internal functions)
	static EthBuffer chunk;


public:
	static uint8_t begin(uint8_t cspin);
	static void loop();
	

	static void staticSetup(IPAddress & ip);
	static MACAddress* whoHas(IPAddress& ip);
	static void enableBroadcast(bool temporary = false);
	static bool isLinkUp();

	static uint8_t getSlot();
	static void freeSlot(uint8_t slotId);

	static uint16_t checksum(uint16_t sum, const uint8_t *data, uint16_t len, bool &carry, bool& odd);
	static uint16_t checksum(uint16_t sum, const uint8_t *data, uint16_t len);

	static void sendIPPacket();



private:

	static void processChunk(uint8_t& handler, uint16_t len);
	static void processTCPSegment(bool isHeader, uint16_t len);

	static uint16_t packetReceiveChunk();
	static void makeWhoHasARPRequest(IPAddress& ip);
	static void processARPReply();
	static void tick();
	static void registerSocket(Socket& socket);
	static void unregisterSocket(Socket&);

	static Socket* currentSocket;

	static uint8_t availableSlots;
	static uint16_t availableSlotBitmap;

	static Socket* sockets[MAX_TCP_SOCKETS];
};

typedef EtherSocket eth;

#endif
