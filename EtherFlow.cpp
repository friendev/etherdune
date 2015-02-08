
#include <ACross/ACross.h>
#include "EtherFlow.h"
#include <stdarg.h>
#include "Checksum.h"

#include <ACross/SPI.h>



uint8_t selectPin;
static byte Enc28j60Bank;
static int gNextPacketPtr;

bool EtherFlow::broadcast_enabled = false;



void initSPI() 
{
	ACross::SPI::init();
}

//static void enableChip() {
//	cli();
//	digitalWrite(selectPin, LOW);
//}
//
//static void disableChip() {
//	digitalWrite(selectPin, HIGH);
//	sei();
//}

//static void xferSPI(byte data) {
//	SPDR = data;
//	while (!(SPSR&(1 << SPIF)))
//		;
//}

static byte readOp(byte op, byte address) {
	//enableChip();
	//xferSPI(op | (address & ADDR_MASK));
	//xferSPI(0x00);
	//if (address & 0x80)
	//	xferSPI(0x00);
	//byte result = SPDR;
	//disableChip();
	//return result;

	uint8_t b[3];
	b[0] = op | (address & ADDR_MASK);
	b[1] = 0;
	b[2] = 0;

	uint16_t sendLength = (address & 0x80) ? 3 : 2;

	ACross::SPI::sendReceive(selectPin, sendLength, b, 1, b);

	return b[0];

}



static void writeOp(byte op, byte address, byte data) {
	//enableChip();
	//xferSPI(op | (address & ADDR_MASK));
	//xferSPI(data);
	//disableChip();

	byte b[2];

	b[0] = op | (address & ADDR_MASK);
	b[1] = data;
	ACross::SPI::send(selectPin, 2, b);
}

static void SetBank(byte address) {
	if ((address & BANK_MASK) != Enc28j60Bank) {
		writeOp(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_BSEL1 | ECON1_BSEL0);
		Enc28j60Bank = address & BANK_MASK;
		writeOp(ENC28J60_BIT_FIELD_SET, ECON1, Enc28j60Bank >> 5);
	}
}

static byte readRegByte(byte address) {
	SetBank(address);
	return readOp(ENC28J60_READ_CTRL_REG, address);
}

static uint16_t readReg(byte address) {
	return readRegByte(address) + (readRegByte(address + 1) << 8);
}


static void writeRegByte(byte address, byte data) {
	SetBank(address);
	writeOp(ENC28J60_WRITE_CTRL_REG, address, data);
}

static void writeReg(byte address, uint16_t data) {
	writeRegByte(address, data);
	writeRegByte(address + 1, data >> 8);
}

void EtherFlow::readBuf(uint16_t len, byte* data) {
	//enableChip();
	//xferSPI(ENC28J60_READ_BUF_MEM);
	//while (len--) {
	//	xferSPI(0x00);
	//	*data++ = SPDR;
	//}
	//disableChip();

	uint8_t b = ENC28J60_READ_BUF_MEM;
	ACross::SPI::sendReceive(selectPin, 1, &b, len, data);


}

void EtherFlow::readBuf(uint16_t src, uint16_t len, byte* data)
{
	writeReg(ERDPT, src);
	readBuf(len, data);
}

void EtherFlow::writeBuf(uint16_t len, const byte* data) 
{
	//enableChip();
	//xferSPI(ENC28J60_WRITE_BUF_MEM);
	//while (len--)
	//	xferSPI(*data++);
	//disableChip();

	uint8_t* b = new uint8_t[len + 1];
	b[0] = ENC28J60_WRITE_BUF_MEM;
	memcpy(b + 1, data, len);
	ACross::SPI::send(selectPin, len + 1, b);

}

void EtherFlow::writeBuf(uint16_t dst, uint16_t len, const byte* data)
{
	writeReg(EWRPT, dst);
	//writeOp(ENC28J60_WRITE_BUF_MEM, 0, 0x00);
	writeBuf(len, data);

	//writeReg(ERDPT, txStart);
}

void EtherFlow::writeByte(byte b)
{
	//writeOp(ENC28J60_WRITE_BUF_MEM, 0, b);
	writeBuf(1, &b);
}
void EtherFlow::writeByte(uint16_t dst, byte b)
{
	writeBuf(dst, 1, &b);
}

byte EtherFlow::readByte(uint16_t src)
{
	byte b;
	readBuf(src, 1, &b);
	return b;
}


void EtherFlow::moveMem(uint16_t dest, uint16_t src, uint16_t len)
{

	//as ENC28J60 DMA is unable to copy single bytes:
	
	if (len == 0)
		return;

	if (len == 1)
	{
		writeByte(dest, readByte(src));
	}
	else
	{
		// calculate address of last byte
		uint16_t last= len+ src - 1;

		/*  1. Appropriately program the EDMAST, EDMAND
		and EDMADST register pairs. The EDMAST
		registers should point to the first byte to copy
		from, the EDMAND registers should point to the
		last byte to copy and the EDMADST registers
		should point to the first byte in the destination
		range. The destination range will always be
		linear, never wrapping at any values except from
		8191 to 0 (the 8-Kbyte memory boundary).
		Extreme care should be taken when
		programming the start and end pointers to
		prevent a never ending DMA operation which
		would overwrite the entire 8-Kbyte buffer.
		*/
		writeReg(EDMASTL, src);
		writeReg(EDMADSTL, dest);

		if ((src <= RXSTOP_INIT) && (last > RXSTOP_INIT))
			last -= (RXSTOP_INIT - RXSTART_INIT);
		
		writeReg(EDMANDL, last);

		/*
		2. If an interrupt at the end of the copy process is
		desired, set EIE.DMAIE and EIE.INTIE and
		clear EIR.DMAIF.

		3. Verify that ECON1.CSUMEN is clear. */
		writeOp(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_CSUMEN);

		/* 4. Start the DMA copy by setting ECON1.DMAST. */
		writeOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_DMAST);

		// wait until runnig DMA is completed
		while (readOp(ENC28J60_READ_CTRL_REG, ECON1) & ECON1_DMAST);
	}
}




void EtherFlow::packetSend(uint16_t len)
{
	// see http://forum.mysensors.org/topic/536/
	// while (readOp(ENC28J60_READ_CTRL_REG, ECON1) & ECON1_TXRTS)
	if (readRegByte(EIR) & EIR_TXERIF) {
		writeOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_TXRST);
		writeOp(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_TXRST);
		writeOp(ENC28J60_BIT_FIELD_CLR, EIR, EIR_TXERIF);
	}

	writeByte(TXSTART_INIT, 0x00); // set the control byte to zero.
	
	writeReg(ETXST, TXSTART_INIT);
	writeReg(ETXND, TXSTART_INIT + len - 1 + 1); // to include the control byte.

	writeOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_TXRTS);

}

void EtherFlow::packetSend(uint16_t len, const byte* data)
{
	writeBuf(TXSTART_INIT_DATA, len, data);
	packetSend(len);
}

void EtherFlow::enableBroadcast(bool temporary) {
	writeRegByte(ERXFCON, readRegByte(ERXFCON) | ERXFCON_BCEN);
	if (!temporary)
		broadcast_enabled = true;
}

static void writePhy(byte address, uint16_t data) {
	writeRegByte(MIREGADR, address);
	writeReg(MIWR, data);
	while (readRegByte(MISTAT) & MISTAT_BUSY)
		;
}

static uint16_t readPhyByte(byte address) {
	writeRegByte(MIREGADR, address);
	writeRegByte(MICMD, MICMD_MIIRD);
	while (readRegByte(MISTAT) & MISTAT_BUSY)
		;
	writeRegByte(MICMD, 0x00);
	return readRegByte(MIRD + 1);
}

bool EtherFlow::isLinkUp() {
	return (readPhyByte(PHSTAT2) >> 2) & 1;
}

uint8_t EtherFlow::begin(uint8_t cspin)
{
	
	
	
	
//	if (bitRead(SPCR, SPE) == 0)
	
	initSPI();
	selectPin = cspin;
	pinMode(selectPin, OUTPUT);
	pinMode(selectPin, HIGH);


	writeOp(ENC28J60_SOFT_RESET, 0, ENC28J60_SOFT_RESET);
	delay(2); // errata B7/2
	while (!readOp(ENC28J60_READ_CTRL_REG, ESTAT) & ESTAT_CLKRDY)
		;

	gNextPacketPtr = RXSTART_INIT;
	writeReg(ERXST, RXSTART_INIT);
	writeReg(ERXRDPT, RXSTART_INIT);
	writeReg(ERXND, RXSTOP_INIT);
	writeReg(ETXST, TXSTART_INIT);
	writeReg(ETXND, TXSTOP_INIT);
	enableBroadcast(); // change to add ERXFCON_BCEN recommended by epam
	writeReg(EPMM0, 0x303f);
	writeReg(EPMCS, 0xf7f9);
	writeRegByte(MACON1, MACON1_MARXEN | MACON1_TXPAUS | MACON1_RXPAUS);
	writeRegByte(MACON2, 0x00);
	writeOp(ENC28J60_BIT_FIELD_SET, MACON3,
		MACON3_PADCFG0 | MACON3_TXCRCEN | MACON3_FRMLNEN);
	writeReg(MAIPG, 0x0C12);
	writeRegByte(MABBIPG, 0x12);
	writeReg(MAMXFL, MAX_FRAMELEN);
	writeRegByte(MAADR5, NetworkService::localMAC.b[0]);
	writeRegByte(MAADR4, NetworkService::localMAC.b[1]);
	writeRegByte(MAADR3, NetworkService::localMAC.b[2]);
	writeRegByte(MAADR2, NetworkService::localMAC.b[3]);
	writeRegByte(MAADR1, NetworkService::localMAC.b[4]);
	writeRegByte(MAADR0, NetworkService::localMAC.b[5]);
	writePhy(PHCON2, PHCON2_HDLDIS);
	SetBank(ECON1);
	writeOp(ENC28J60_BIT_FIELD_SET, EIE, EIE_INTIE | EIE_PKTIE);
	writeOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_RXEN);

	byte rev = readRegByte(EREVID);
	// microchip forgot to step the number on the silcon when they
	// released the revision B7. 6 is now rev B7. We still have
	// to see what they do when they release B8. At the moment
	// there is no B8 out yet
	if (rev > 5) ++rev;
	return rev;



}

uint16_t EtherFlow::packetReceiveChunk()
{
	uint16_t len = 0;
	if (readRegByte(EPKTCNT) > 0)
	{
		writeReg(ERDPT, gNextPacketPtr);

		struct
		{
			uint16_t nextPacket;
			uint16_t byteCount;
			uint16_t status;
		} header;

		readBuf(sizeof header, (byte*)&header);
		uint16_t ptr = gNextPacketPtr + sizeof(header);


		gNextPacketPtr = header.nextPacket;
		len = header.byteCount - 4; //remove the CRC count

		if ((header.status & 0x80) == 0)
			len = 0;

		uint16_t chunkLength;
		bool isHeader = true;
		
		
		while (len > 0)
		{
			chunkLength = min(sizeof(EthBuffer), len);
			
			readBuf(ptr,chunkLength,(byte*) &NetworkService::chunk);

			if (!NetworkService::processChunk(isHeader,chunkLength))
				break;

			isHeader = false;

			len -= chunkLength;
			ptr += chunkLength;
		}

		if (gNextPacketPtr - 1 > RXSTOP_INIT)
			writeReg(ERXRDPT, RXSTOP_INIT);
		else
			writeReg(ERXRDPT, gNextPacketPtr - 1);
		writeOp(ENC28J60_BIT_FIELD_SET, ECON2, ECON2_PKTDEC);
	}
	return len;

}


void EtherFlow::loop()
{
	packetReceiveChunk();
}



