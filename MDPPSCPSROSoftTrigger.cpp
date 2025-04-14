/*
    This software is Copyright by the Board of Trustees of Michigan
    State University (c) Copyright 2024.

    You may use this software under the terms of the GNU public license
    (GPL).  The terms of this license are described at:

     http://www.gnu.org/licenses/gpl.txt

     Authors:
             Genie Jhang
	     FRIB
	     Michigan State University
	     East Lansing, MI 48824-1321
*/

#include <CDataSource.h>        // Abstract source of ring items.
#include <CDataSourceFactory.h> // Turn a URI into a concrete data source
#include <CDataSink.h>          // Abstract sink of ring items.
#include <CDataSinkFactory.h>   // Turn a URI into a concrete data sink.
#include <CRingItem.h>          // Base class for ring items.
#include <CPhysicsEventItem.h>  // CPhysicsEventItem class for PHYSICS_EVENT items.
#include <DataFormat.h>         // Ring item data formats.
#include <Exception.h>          // Base class for exception handling.

#include <fstream>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <vector>
#include <cstdint>
#include <queue>
#include <deque>

#include "MDPPSCPSRO.h"

//double                 MDPP_TDC_UNIT = 24.41; // ps
double                 MDPP_TDC_UNIT = 781.25; // ps
uint64_t                MDPP_TDC_MAX = 0x3FFFFFFFFFFF;
double         MDPP_TIMESTAMP_MAX_NS = static_cast<double>(MDPP_TDC_MAX)*MDPP_TDC_UNIT/1000.;
double    REVERSED_TEST_THRESHOLD_NS = 10; // ns

#define NUM_CHANNEL 32

#define DEBUG

using std::queue;
using std::deque;
using std::cout;
using std::cerr;
using std::endl;

class MDPPSCPSROSoftTrigger {
	public:
		MDPPSCPSROSoftTrigger() {};
		~MDPPSCPSROSoftTrigger() {};

	public:
    bool timeSet = false;

  double     mdppTimestamp_ns = 0;
  double prevMdppTimestamp_ns = 0;
uint64_t latestAbsoluteMdppTimestamp    = 0;
  double latestAbsoluteMdppTimestamp_ns = 0;

uint64_t  mdppRolloverCounter = 0;

	double refDiff_ns = 0;

     int  triggerChannel = -1;
  double  windowStart_ns = -1; // valid always positive
  double  windowWidth_ns = -1; // valid always positive
uint64_t  windowStart    = 0; // derived from ns approx value in MDPP_TDC_UNIT
uint64_t  windowWidth    = 0; // derived from ns approx value in MDPP_TDC_UNIT

    bool dataCollecting = false;
    bool collectingDone = true;
  double  windowStartTimestamp_ns = 0;
  double    windowEndTimestamp_ns = 0;
uint64_t  windowStartTimestamp    = 0;
uint64_t    windowEndTimestamp    = 0;

deque<MDPPSCPSRO *> hitDeque;
queue<MDPPSCPSRO *> eventQueue;

	public:
uint64_t getMdppTimestamp(MDPPSCPSRO &anEvent);
double getMdppTimestamp_ns(MDPPSCPSRO &anEvent);
uint64_t getAbsoluteMdppTimestamp(MDPPSCPSRO &anEvent);
double getAbsoluteMdppTimestamp_ns(MDPPSCPSRO &anEvent);
MDPPSCPSRO &unpack(CRingItem &item);
CPhysicsEventItem *pack(MDPPSCPSRO &anEvent);
void send(CDataSink &sink, CRingItem &item);
void updateTimestamps(MDPPSCPSRO &anEvent);
MDPPSCPSRO &getLastEvent();
MDPPSCPSRO &getFirstEvent();
MDPPSCPSRO &peekFirstEvent();
void collectEvent(MDPPSCPSRO &anEvent);
void sendCollection(CDataSink &sink);
void updateTriggerWindow(MDPPSCPSRO &triggerEvent);
void sending(CDataSink &sink, bool isTriggerChannel);
void emptyingQueues(CDataSink &sink);
};

/**
 * Usage:
 *    This outputs an error message that shows how the program should be used
 *     and exits using std::exit().
 *
 * @param o   - references the stream on which the error message is printed.
 * @param msg - Error message that precedes the usage information.
 */
void usage(std::ostream &o, const char *msg, const char *program)
{
	o << msg << std::endl;
	o << "= Usage:\n";
	o << "  " << program << " inRingURI outRingURI trigCh winStart winWidth\n";
	o << "        inRingURI - the file: or tcp: URI that describes where data comes from\n";
	o << "       outRingURI - the file: or tcp: URI that describes where data goes out to\n";
	o << "           trigCh - a channel number to create trigger window\n";
	o << "         winStart - trigger window start time in ns (WS)\n";
	o << "         winWidth - trigger window width in ns (WW)\n\n";
	o << "       Trigger window is created as (t_ch - WS, t_ch - WS + WW).\n";

	std::exit(EXIT_FAILURE);
}

uint64_t MDPPSCPSROSoftTrigger::getMdppTimestamp(MDPPSCPSRO &anEvent)
{
	return anEvent.timestamp;
}

double MDPPSCPSROSoftTrigger::getMdppTimestamp_ns(MDPPSCPSRO &anEvent)
{
	return static_cast<double>(anEvent.timestamp)*MDPP_TDC_UNIT/1000.;
}

uint64_t MDPPSCPSROSoftTrigger::getAbsoluteMdppTimestamp(MDPPSCPSRO &anEvent)
{
	uint64_t absoluteMdppTimestamp = (anEvent.rollovercounter << 46) | getMdppTimestamp(anEvent);
	return absoluteMdppTimestamp;
}

double MDPPSCPSROSoftTrigger::getAbsoluteMdppTimestamp_ns(MDPPSCPSRO &anEvent)
{
	return static_cast<double>(getAbsoluteMdppTimestamp(anEvent))*MDPP_TDC_UNIT/1000.;
}

MDPPSCPSRO &MDPPSCPSROSoftTrigger::unpack(CRingItem &item) {
	std::unique_ptr<CRingItem> pItem(&item);

	MDPPSCPSRO *pAnEvent = new MDPPSCPSRO();
	MDPPSCPSRO &anEvent = *pAnEvent;

	void *p = item.getBodyPointer();

	uint16_t *vmusbHeader = reinterpret_cast<uint16_t *>(p);
	anEvent.stackid = ((*vmusbHeader)&0xe000) >> 13;
	anEvent.bodysize = (*vmusbHeader)&0x0FFF;

#ifdef DEBUG
	cout << "vmusbHeader: " << std::hex << "0x" << *vmusbHeader<< std::dec << endl;
	cout << "stackID: " << anEvent.stackid << endl;
	cout << "bodySize: " << anEvent.bodysize << endl;
#endif

	vmusbHeader++;

	if (anEvent.bodysize != 0xc) {
		// This is wrong!
	}

	uint32_t *a32BitItem = reinterpret_cast<uint32_t *>(vmusbHeader);
	int header = (*a32BitItem&0xC0000000) >> 30;

	if (header != 1) {
		anEvent.moduleid = -1;

		return anEvent;
	}

	anEvent.moduleid      = (*a32BitItem &    0xFF0000) >> 16;
	anEvent.tdcresolution = (*a32BitItem &      0xE000) >> 13;

	int num32Words        =  *a32BitItem &       0x3FF;

	// Next 32 bit item - ADC data
	a32BitItem++;

	// Just for checking
	bool dataChecker = (*a32BitItem &  0x10000000) >> 28;
	if (!dataChecker) {
		anEvent.moduleid = -1;

		return anEvent;
	}

	anEvent.pileup   = (*a32BitItem   &  0x1000000) >> 18;
	anEvent.overflow = (*a32BitItem   &   0x800000) >> 17;
	anEvent.ch       = (*a32BitItem   &   0x7F0000) >> 16;
	anEvent.adc      =  *a32BitItem   &     0xffff;

#ifdef DEBUG
	cout << "moduleid: " << anEvent.moduleid << endl;
	cout << "channel: " << anEvent.ch << endl;
	cout << "pileup flag: " << anEvent.pileup << endl;
	cout << "overflow flag: " << anEvent.overflow << endl;
	cout << "adc: " << anEvent.adc<< endl;
#endif

	// Next 32 bit item - Extended timestamp
	a32BitItem++;

	// Just for checking
	dataChecker = (*a32BitItem &  0x20000000) >> 29;
	if (!dataChecker) {
		anEvent.moduleid = -1;

		return anEvent;
	}

	anEvent.timestamp = (static_cast<uint64_t>(*a32BitItem & 0xFFFF) << 30);

	// Next 32 bit item - timestamp
	a32BitItem++;

	// Just for checking
	dataChecker = (((*a32BitItem &  0xC0000000) >> 30) == 3);
	if (!dataChecker) {
		anEvent.moduleid = -1;

		return anEvent;
	}

	anEvent.timestamp |= (*a32BitItem & 0x3FFFFFFF);

#ifdef DEBUG
	cout << "timestamp: " << anEvent.timestamp << endl;
#endif

	return anEvent;
}

CPhysicsEventItem *MDPPSCPSROSoftTrigger::pack(MDPPSCPSRO &anEvent)
{
	std::unique_ptr<MDPPSCPSRO> pAnEvent(&anEvent);

	CPhysicsEventItem *newItem = new CPhysicsEventItem();

	void *dest = newItem -> getBodyCursor();

	uint16_t num32BitItems = 4;
	uint16_t bodySize = 0x1 + num32BitItems*2 + 0x4;
	uint16_t vmusbHeader = ((anEvent.stackid&0x7) << 13) | (bodySize&0xFFF);

	std::memcpy(dest, &vmusbHeader, 2);
	dest = static_cast<void *>(static_cast<uint8_t *>(dest) + 2);

	uint64_t headerItem = (0x1                              << 30)
											| ((anEvent.moduleid      &   0xFF) << 16)
											| ((anEvent.tdcresolution &    0x7) << 13)
											|  (num32BitItems - 1     &  0x3FF);

	std::memcpy(dest, &headerItem, 4);
	dest = static_cast<void *>(static_cast<uint8_t *>(dest) + 4);

	uint64_t adcItem = (0x1                        << 28)
									|  (anEvent.pileup             << 24)
									|  (anEvent.overflow           << 23)
									| ((anEvent.ch       &   0x7F) << 16)
									|  (anEvent.adc      & 0xFFFF);

	std::memcpy(dest, &adcItem, 4);
	dest = static_cast<void *>(static_cast<uint8_t *>(dest) + 4);

	uint64_t timestampHighItem = (0x2                                         << 28)
		 												| ((anEvent.timestamp & 0x3FFFC0000000) >> 30);

	std::memcpy(dest, &timestampHighItem, 4);
	dest = static_cast<void *>(static_cast<uint8_t *>(dest) + 4);

	uint64_t timestampLowItem = (0x3                             << 30)
		 												| (anEvent.timestamp & 0x3FFFFFFF);

	std::memcpy(dest, &timestampLowItem, 4);
	dest = static_cast<void *>(static_cast<uint8_t *>(dest) + 4);

	uint64_t ender = 0xFFFFFFFF;

	std::memcpy(dest, &ender, 4);
	dest = static_cast<void *>(static_cast<uint8_t *>(dest) + 4);

	std::memcpy(dest, &ender, 4);
	dest = static_cast<void *>(static_cast<uint8_t *>(dest) + 4);

	newItem -> setBodyCursor(dest);
	newItem -> updateSize();

	return newItem;
}

void MDPPSCPSROSoftTrigger::send(CDataSink &sink, CRingItem &item)
{
	std::unique_ptr<CRingItem> pItem(&item);

	sink.putItem(*pItem);
}

void MDPPSCPSROSoftTrigger::updateTimestamps(MDPPSCPSRO &anEvent)
{
	prevMdppTimestamp_ns = mdppTimestamp_ns;
			mdppTimestamp_ns = getMdppTimestamp_ns(anEvent);
	double mdppTimestampDiff_ns = mdppTimestamp_ns - prevMdppTimestamp_ns;
	if (!timeSet) {
		mdppTimestampDiff_ns = 0;

		anEvent.rollovercounter = mdppRolloverCounter;
		latestAbsoluteMdppTimestamp    = getAbsoluteMdppTimestamp(anEvent);
		latestAbsoluteMdppTimestamp_ns = getAbsoluteMdppTimestamp_ns(anEvent);

		timeSet = true;

		return;
	}

#ifdef DEBUG
				cerr << "               MDPP timestamp diff in ns: " << mdppTimestampDiff_ns << endl;
#endif

	if (mdppTimestampDiff_ns < 0) {
		if (prevMdppTimestamp_ns > MDPP_TIMESTAMP_MAX_NS/2 && mdppTimestamp_ns < MDPP_TIMESTAMP_MAX_NS/2) {
			mdppRolloverCounter += 1;

#ifdef DEBUG
				cerr << "== Rolled over ==" << endl;
				cerr << "                         rolloverCounter: " << mdppRolloverCounter << endl;
#endif
		} else {
#ifdef DEBUG
				cerr << "== Reversed order event! ==" << endl;
				cerr << "                    mdppTimestampDiff_ns: " << mdppTimestampDiff_ns << endl;
#endif
		}
	}

	anEvent.rollovercounter = mdppRolloverCounter;
	latestAbsoluteMdppTimestamp    = std::max(getAbsoluteMdppTimestamp(anEvent), latestAbsoluteMdppTimestamp);
	latestAbsoluteMdppTimestamp_ns = std::max(getAbsoluteMdppTimestamp_ns(anEvent), latestAbsoluteMdppTimestamp_ns);

#ifdef DEBUG
				cerr << "         latest absolute MDPP timestamps: " << latestAbsoluteMdppTimestamp << endl;
				cerr << "   latest absolute MDPP timestamps in ns: " << latestAbsoluteMdppTimestamp_ns << endl;
#endif
}

MDPPSCPSRO &MDPPSCPSROSoftTrigger::getLastEvent()
{
	MDPPSCPSRO *pAnEvent = hitDeque.back();
	hitDeque.pop_back();

	return *pAnEvent;
}

MDPPSCPSRO &MDPPSCPSROSoftTrigger::getFirstEvent()
{
	MDPPSCPSRO *pAnEvent = hitDeque.front();
	hitDeque.pop_front();

	return *pAnEvent;
}

MDPPSCPSRO &MDPPSCPSROSoftTrigger::peekFirstEvent()
{
	return *hitDeque.front();
}

void MDPPSCPSROSoftTrigger::collectEvent(MDPPSCPSRO &anEvent)
{
	eventQueue.push(&anEvent);

	dataCollecting = true;
}

void MDPPSCPSROSoftTrigger::sendCollection(CDataSink &sink)
{
	MDPPSCPSRO &anEvent = *eventQueue.front();

//	CPhysicsEventItem *pNewItem = new CPhysicsEventItem(anEvent.eventtimestamp, anEvent.sourceid, 0, 8192);
	CPhysicsEventItem *pNewItem = new CPhysicsEventItem();
	CPhysicsEventItem &newItem = *pNewItem;

	void *dest = newItem.getBodyCursor();

	uint16_t bodySize = 0x1 + 8*eventQueue.size() + 4; // an event(0xc)*#events + ender
	uint16_t vmusbHeader = ((anEvent.stackid&0x7) << 13) | (bodySize&0xFFF);

	std::memcpy(dest, &vmusbHeader, 2);
	dest = static_cast<void *>(static_cast<uint8_t *>(dest) + 2);

	while (!eventQueue.empty()) {
    std::unique_ptr<MDPPSCPSRO> pAnEvent(eventQueue.front());
		MDPPSCPSRO &anEvent = *pAnEvent;

		uint64_t headerItem = (0x1                              << 30)
												| ((anEvent.moduleid      &   0xFF) << 16)
											 	| ((anEvent.tdcresolution &    0x7) << 13)
											 	|  (0x3                   &  0x3FF);

		std::memcpy(dest, &headerItem, 4);
		dest = static_cast<void *>(static_cast<uint8_t *>(dest) + 4);

		uint64_t adcItem = (0x1                        << 28)
										|  (anEvent.pileup             << 24)
									 	|  (anEvent.overflow           << 23)
									 	| ((anEvent.ch       &   0x7F) << 16)
									 	|  (anEvent.adc      & 0xFFFF);

		std::memcpy(dest, &adcItem, 4);
		dest = static_cast<void *>(static_cast<uint8_t *>(dest) + 4);

		uint64_t timestampHighItem = (0x2                                              << 28)
															|  (anEvent.rollovercounter                          << 16)
															| ((anEvent.timestamp       & 0x3FFFC0000000) >> 30);

		std::memcpy(dest, &timestampHighItem, 4);
		dest = static_cast<void *>(static_cast<uint8_t *>(dest) + 4);

		uint64_t timestampLowItem = (0x3                             << 30)
															| (anEvent.timestamp & 0x3FFFFFFF);

		std::memcpy(dest, &timestampLowItem, 4);
		dest = static_cast<void *>(static_cast<uint8_t *>(dest) + 4);

		eventQueue.pop();
	}

	uint64_t ender = 0xFFFFFFFF;

	std::memcpy(dest, &ender, 4);
	dest = static_cast<void *>(static_cast<uint8_t *>(dest) + 4);

	std::memcpy(dest, &ender, 4);
	dest = static_cast<void *>(static_cast<uint8_t *>(dest) + 4);

	newItem.setBodyCursor(dest);
	newItem.updateSize();

	send(sink, newItem);

	dataCollecting = false;
}

void MDPPSCPSROSoftTrigger::updateTriggerWindow(MDPPSCPSRO &triggerEvent)
{
	windowStartTimestamp    = getAbsoluteMdppTimestamp(triggerEvent) - windowStart;
	windowStartTimestamp_ns = static_cast<double>(windowStartTimestamp)*MDPP_TDC_UNIT/1000.;
	if (getAbsoluteMdppTimestamp(triggerEvent) < windowStart) {
		windowStartTimestamp    = 0;
		windowStartTimestamp_ns = 0;
	}
	windowEndTimestamp    = windowStartTimestamp + windowWidth;
	windowEndTimestamp_ns = static_cast<double>(windowEndTimestamp)*MDPP_TDC_UNIT/1000.;
}

void MDPPSCPSROSoftTrigger::sending(CDataSink &sink, bool isTriggerChannel)
{
	if (isTriggerChannel && !dataCollecting) {
		MDPPSCPSRO &triggerEvent = getLastEvent();

		updateTriggerWindow(triggerEvent);

#ifdef DEBUG
				cout << "== New trigger event detected ==" << endl;
				cout << "                           hitDeque size: " << hitDeque.size() << endl;
				cout << "            Window start timestamp in ns: " << windowStartTimestamp_ns << " (" << windowStartTimestamp << ")" << endl;
				cout << "           MDPP absolute timestamp in ns: " << getAbsoluteMdppTimestamp_ns(triggerEvent) << " (" << getAbsoluteMdppTimestamp(triggerEvent) << ")" << endl;
#endif

		while (!hitDeque.empty()) {
			MDPPSCPSRO &anEvent = getFirstEvent();

			if (getAbsoluteMdppTimestamp(anEvent) >= windowStartTimestamp && getAbsoluteMdppTimestamp(anEvent) <= windowEndTimestamp)
		 	{
#ifdef DEBUG
				cout << "== Collected before trigger event ==" << endl;
				cout << "  MDPP timestamp from window start in ns: " << getAbsoluteMdppTimestamp_ns(anEvent) - windowStartTimestamp_ns << " (" << getAbsoluteMdppTimestamp(anEvent) - windowStartTimestamp << ")" << endl;
#endif
				collectEvent(anEvent);
			}
			else if (getAbsoluteMdppTimestamp(anEvent) < windowStartTimestamp)
			{
#ifdef DEBUG
				cout << "== Flushing before window start event ==" << endl;
				cout << "            Window start timestamp in ns: " << windowStartTimestamp_ns << " (" << windowStartTimestamp << ")" << endl;
				cout << "                    MDPP timestamp in ns: " << getAbsoluteMdppTimestamp_ns(anEvent) << " (" << getAbsoluteMdppTimestamp(anEvent) << ")" << endl;
#endif
				CPhysicsEventItem &packedEvent = *pack(anEvent);
				send(sink, packedEvent);
			}
		 	else 
			{
				cerr << "== This shouldn't be happening! 1 ==" << endl;
				cerr << "  MDPP timestamp from window start in ns: " << getAbsoluteMdppTimestamp_ns(anEvent) - windowStartTimestamp_ns << " (" << getAbsoluteMdppTimestamp(anEvent) - windowStartTimestamp << ")" << endl;
				cerr << "                      Window start in ns: " << windowStartTimestamp_ns << " (" << windowStartTimestamp << ")" << endl;
				cout << "           MDPP absolute timestamp in ns: " << getAbsoluteMdppTimestamp_ns(triggerEvent) << " (" << getAbsoluteMdppTimestamp(triggerEvent) << ")" << endl;
				cerr << "                   MDPP rollover counter: " << anEvent.rollovercounter << endl;
				cerr << "                          MDPP timestamp: " << anEvent.timestamp << endl;

				break;
			}
		}

#ifdef DEBUG
				cout << "== Collected trigger event ==" << endl;
				cout << "  MDPP timestamp from window start in ns: " << getAbsoluteMdppTimestamp_ns(triggerEvent) - windowStartTimestamp_ns << " (" << getAbsoluteMdppTimestamp(triggerEvent) - windowStartTimestamp << ")" << endl;
				cout << "                    MDPP timestamp in ns: " << getAbsoluteMdppTimestamp_ns(triggerEvent) << " (" << getAbsoluteMdppTimestamp(triggerEvent) << ")" << endl;
#endif

		collectEvent(triggerEvent);
	} else if (dataCollecting) {
		MDPPSCPSRO &anEvent = peekFirstEvent();

#ifdef DEBUG
				cout << "== Collecting? ==" << endl;
				cout << "  MDPP timestamp from window start in ns: " << getAbsoluteMdppTimestamp_ns(anEvent) - windowStartTimestamp_ns << " (" << getAbsoluteMdppTimestamp(anEvent) - windowStartTimestamp << ")" << endl;
				cout << "                    MDPP timestamp in ns: " << getAbsoluteMdppTimestamp_ns(anEvent) << " (" << getAbsoluteMdppTimestamp(anEvent) << ")" << endl;
				cout << "             windowStart timestamp in ns: " << windowStartTimestamp_ns << " (" << windowStartTimestamp << ")" << endl;
				cout << "               windowEnd timestamp in ns: " << windowEndTimestamp_ns << " (" << windowEndTimestamp << ")" << endl;
#endif
		if (getAbsoluteMdppTimestamp(anEvent) >= windowStartTimestamp && getAbsoluteMdppTimestamp(anEvent) <= windowEndTimestamp)
		{
			anEvent = getFirstEvent();

#ifdef DEBUG
				cout << "== Collected after trigger event ==" << endl;
				cout << "  MDPP timestamp from window start in ns: " << getAbsoluteMdppTimestamp_ns(anEvent) - windowStartTimestamp_ns << " (" << getAbsoluteMdppTimestamp(anEvent) - windowStartTimestamp << ")" << endl;
				cout << "                    MDPP timestamp in ns: " << getAbsoluteMdppTimestamp_ns(anEvent) << " (" << getAbsoluteMdppTimestamp(anEvent) << ")" << endl;
#endif

			collectEvent(anEvent);
		}
		else if (windowEndTimestamp < latestAbsoluteMdppTimestamp)
		{
#ifdef DEBUG
				cout << "== Collecting done! Sending ==" << endl;
#endif
			sendCollection(sink);

#ifdef DEBUG
				cout << "== Checking if there's trigger event left ==" << endl;
#endif

			sending(sink, anEvent.ch == triggerChannel);
		}
		else
		{
			cerr << "== This shouldn't be happening! 2 ==" << endl;
			cerr << "  MDPP timestamp from window start in ns: " << getAbsoluteMdppTimestamp_ns(anEvent) - windowStartTimestamp_ns << " (" << getAbsoluteMdppTimestamp(anEvent) - windowStartTimestamp << ")" << endl;
			cerr << "                      Window start in ns: " << windowStartTimestamp_ns << " (" << windowStartTimestamp << ")" << endl;
			cerr << "           MDPP absolute timestamp in ns: " << getAbsoluteMdppTimestamp_ns(anEvent) << " (" << getAbsoluteMdppTimestamp(anEvent) << ")" << endl;
			cerr << "                   MDPP rollover counter: " << anEvent.rollovercounter << endl;
			cerr << "                          MDPP timestamp: " << anEvent.timestamp << endl;
		}
	}	else {
		while (!hitDeque.empty()) {
			MDPPSCPSRO &anEvent = peekFirstEvent();

			if (latestAbsoluteMdppTimestamp - getAbsoluteMdppTimestamp(anEvent) > windowStart)
			{
#ifdef DEBUG
				cout << "== Too far from the window start ==" << endl;
				cout << "                    MDPP timestamp in ns: " << getAbsoluteMdppTimestamp_ns(anEvent) << " (" << getAbsoluteMdppTimestamp(anEvent) << ")" << endl;
				cout << "                  latest timestamp in ns: " << latestAbsoluteMdppTimestamp_ns << " (" << latestAbsoluteMdppTimestamp << ")" << endl;
#endif
				anEvent = getFirstEvent();
				CPhysicsEventItem &packedEvent = *pack(anEvent);
				send(sink, packedEvent);
			} else {
				break;
			}
		}
	}
}

void MDPPSCPSROSoftTrigger::emptyingQueues(CDataSink &sink)
{
#ifdef DEBUG
				cout << "== Emptying for ending ==" << endl;
#endif
	if (!eventQueue.empty()) {
		sendCollection(sink);
	}

	while (!hitDeque.empty()) {
		MDPPSCPSRO &anEvent = getFirstEvent();

		CPhysicsEventItem &packedEvent = *pack(anEvent);
		send(sink, packedEvent);
	}
}

/**
 * The main program:
 *    - Ensures we have a URI parameter (and only a URI parameter).
 *    - Constructs a data source.
 *    - Reads items from the data source until the data source is exhausted.
 *
 *  @note Online ringbuffers are never exhausted.  The program will just block
 *        when the ringbuffer is empty until the ring has new data.
 *  @note Because of the note above, this function never exits for online sources.
 */
int main(int argc, char **argv)
{
	MDPPSCPSROSoftTrigger *core = new MDPPSCPSROSoftTrigger();
	// Make sure we have enough command line parameters.

	if (argc != 6) {
		usage(std::cerr, "Not enough command line parameters", argv[0]);
	}

	// Create the data source.   Data sources allow us to specify ring item
	// types that will be skipped.  They also allow us to specify types
	// that we may only want to sample (e.g. for online ring items).

	std::vector<std::uint16_t> sample;     // Insert the sampled types here.
	std::vector<std::uint16_t> exclude;    // Insert the skippable types here.
	CDataSource* pDataSource;
	try {
		pDataSource = CDataSourceFactory::makeSource(argv[1], sample, exclude);
		std::cout << "==  Connecting to the input RingBuffer: " << argv[1] << std::endl;
	}
	catch (CException &e) {
		std::cerr << "Failed to open ring source\b" << std::endl;
		usage(std::cerr, e.ReasonText(), argv[0]);
	}

	// Create a data sink that can be passed to the data processor.
	// this is wrappedn in an std::unique_ptr to ensure if an exception
	// stops the program it the sink is properly destructed (flushing and closing it).
	//

	CDataSink* pSink;
	try {
		CDataSinkFactory factory;
		pSink = factory.makeSink(argv[2]);
		std::cout << "== Connecting to the output RingBuffer: " << argv[2] << std::endl;
	}
	catch (CException& e) {
		std::cerr << "Failed to create data sink: ";
		usage(std::cerr, e.ReasonText(), argv[0]);
	}
	std::unique_ptr<CDataSink> sink(pSink);

	core -> triggerChannel = atoi(argv[3]);
	core -> windowStart_ns = atof(argv[4]);
	core -> windowWidth_ns = atof(argv[5]);
	core -> windowStart    = core -> windowStart_ns*1000/MDPP_TDC_UNIT;
	core -> windowWidth    = core -> windowWidth_ns*1000/MDPP_TDC_UNIT;

	std::cout << std::endl;
	std::cout << "==  Software trigger channel: " << core -> triggerChannel << std::endl;
	std::cout << "== Trigger window start (ns): " << core -> windowStart_ns << std::endl;
	std::cout << "== Trigger window width (ns): " << core -> windowWidth_ns << std::endl;
	std::cout << std::endl;

	// The loop below consumes items from the ring buffer until
	// all are used up.  The use of an std::unique_ptr ensures that the
	// dynamically created ring items we get from the data source are
	// automatically deleted when we exit the block in which it's created.

	std::cout << "== Starting processing software trigger" << std::endl;

	CRingItem *pItem;
	while ((pItem = pDataSource -> getItem() )) {
		CRingItem &item = *pItem;

		if (item.type() == PHYSICS_EVENT) {
			MDPPSCPSRO &anEvent = core -> unpack(item);

			core -> hitDeque.push_back(&anEvent);
			core -> updateTimestamps(anEvent);
			core -> sending(*sink, anEvent.ch == core -> triggerChannel);
		} else if (item.type() == END_RUN || item.type() == ABNORMAL_ENDRUN) {
			core -> emptyingQueues(*sink);
			core -> send(*sink, item);
		} else if (item.type() == PHYSICS_EVENT_COUNT) {
			std::unique_ptr<CRingItem> upItem(pItem);
		} else {
			core -> send(*sink, item);
		}
	}

	std::cout << "== Ending processing software trigger" << std::endl;
	
	// We can only fall through here for file data sources... normal exit
	std::exit(EXIT_SUCCESS);
}
