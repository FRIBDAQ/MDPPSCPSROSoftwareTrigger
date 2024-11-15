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

#define EXTERNAL_TIMESTAMP_MAX   0xFFFFFFFF // 32bits
#define EXTERNAL_CLOCK_PERIOD_NS 62.5  // ns (16MHz)
#define MDPP_TDC_UNIT            24.41 // ps
#define MDPP_TDC_MAX             0x3FFFFFFF
#define MDPP_TIMESTAMP_MAX_NS    MDPP_TDC_MAX*MDPP_TDC_UNIT/1000.

#define NUM_CHANNEL 32

//#define DEBUG

using std::queue;
using std::deque;
using std::cout;
using std::cerr;
using std::endl;

    bool timeSet = false;

  double     timestamp_ns = 0;
uint64_t prevTimestamp_ns = 0;
uint64_t externalTimestampRolloverCounter = 0;

uint64_t  mdppTimestampRef    = 0;  // in 24.41ps
  double     mdppTimestamp_ns = 0;
  double prevMdppTimestamp_ns = 0;
uint64_t latestAbsoluteMdppTimestamp    = 0;
  double latestAbsoluteMdppTimestamp_ns = 0;

uint64_t  mdppRolloverCounter = 0;

	double refDiff_ns = 0;

     int  triggerChannel = -1;
  double  windowStart_ns = -1; // valid always positive
  double  windowWidth_ns = -1; // valid always positive
uint64_t  windowStart    = 0; // derived from ns approx value in 24.41ps
uint64_t  windowWidth    = 0; // derived from ns approx value in 24.41ps

    bool dataCollecting = false;
    bool collectingDone = true;
  double  windowStartTimestamp_ns = 0;
  double    windowEndTimestamp_ns = 0;
uint64_t  windowStartTimestamp    = 0;
uint64_t    windowEndTimestamp    = 0;

deque<MDPPSCPSRO *> hitDeque;
queue<MDPPSCPSRO *> eventQueue;

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
	o << "  " << program << " uri outringPrefix\n";
	o << "        in ring uri - the file: or tcp: URI that describes where data comes from\n";
	o << "       out ring uri - the file: or tcp: URI that describes where data goes out to\n";
	o << "    trigger channel - a channel number to create trigger window\n";
	o << "       window start - trigger window start time in ns (WS)\n";
	o << "       window width - trigger window width in ns (WW)\n\n";
	o << "       Trigger window is created as (t_ch - WS, t_ch - WS + WW).\n";

	std::exit(EXIT_FAILURE);
}

double getTimestamp_ns(MDPPSCPSRO &anEvent)
{
	return (externalTimestampRolloverCounter*EXTERNAL_TIMESTAMP_MAX + anEvent.externaltimestamp)*EXTERNAL_CLOCK_PERIOD_NS - refDiff_ns;
}

uint32_t getMdppTimestamp(MDPPSCPSRO &anEvent)
{
	return anEvent.timestamp;
}

double getMdppTimestamp_ns(MDPPSCPSRO &anEvent)
{
	return anEvent.timestamp*MDPP_TDC_UNIT/1000.;
}

uint64_t getAbsoluteMdppTimestamp(MDPPSCPSRO &anEvent)
{
	return (anEvent.rollovercounter << 30) | getMdppTimestamp(anEvent);
}

double getAbsoluteMdppTimestamp_ns(MDPPSCPSRO &anEvent)
{
	return anEvent.rollovercounter*MDPP_TIMESTAMP_MAX_NS + getMdppTimestamp_ns(anEvent);
}

MDPPSCPSRO &unpack(CRingItem &item) {
	std::unique_ptr<CRingItem> pItem(&item);

	MDPPSCPSRO *pAnEvent = new MDPPSCPSRO();
	MDPPSCPSRO &anEvent = *pAnEvent;

	void *p = item.getBodyPointer();

//	anEvent.eventtimestamp = item.getEventTimestamp();
//	anEvent.sourceid = item.getSourceId();
	
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

	uint32_t *externalTimestamp = reinterpret_cast<uint32_t *>(vmusbHeader);
	anEvent.externaltimestamp = *externalTimestamp;

#ifdef DEBUG
	cout << "externalTimestamp: " << *externalTimestamp << endl;
#endif

	externalTimestamp++;

	// Skip the second scaler
	externalTimestamp++;

	uint32_t *firstItem = reinterpret_cast<uint32_t *>(externalTimestamp);
	int header = (*firstItem&0xC0000000) >> 30;

	if (header != 1) {
		anEvent.moduleid = -1;

		return anEvent;
	}

	anEvent.moduleid = (*firstItem   & 0x3F000000) >> 24;
	anEvent.trigflag = (*firstItem   &   0x800000) >> 23;
	anEvent.ch       = (*firstItem   &   0x7C0000) >> 18;
	anEvent.pileup   = (*firstItem   &    0x20000) >> 17;
	anEvent.overflow = (*firstItem   &    0x10000) >> 16;
	anEvent.adc      =  *firstItem;

#ifdef DEBUG
	cout << "moduleid: " << anEvent.moduleid << endl;
	cout << "trigger flag: " << anEvent.trigflag << endl;
	cout << "channel: " << anEvent.ch << endl;
	cout << "pileup flag: " << anEvent.pileup << endl;
	cout << "overflow flag: " << anEvent.overflow << endl;
	cout << "adc: " << anEvent.adc<< endl;
#endif

	firstItem++;

	uint32_t *secondItem = firstItem;
	header = (*secondItem&0xC0000000) >> 30;

	if (header != 3) {
		anEvent.moduleid = -1;

		return anEvent;
	}

	anEvent.timestamp = *secondItem&0x3FFFFFFF;

#ifdef DEBUG
	cout << "timestamp: " << anEvent.timestamp << endl;
#endif

	return anEvent;
}

CPhysicsEventItem *pack(MDPPSCPSRO &anEvent)
{
	std::unique_ptr<MDPPSCPSRO> pAnEvent(&anEvent);

//	CPhysicsEventItem *newItem = new CPhysicsEventItem(anEvent.eventtimestamp, anEvent.sourceid, 0, 8192);
	CPhysicsEventItem *newItem = new CPhysicsEventItem();

	void *dest = newItem -> getBodyCursor();

	uint16_t bodySize = 0xc + 4; // Original + zero pad + extended timestamp
	uint16_t vmusbHeader = ((anEvent.stackid&0x7) << 13) | (bodySize&0xFFF);

	std::memcpy(dest, &vmusbHeader, 2);
	dest = static_cast<void *>(static_cast<uint8_t *>(dest) + 2);

	std::memcpy(dest, &anEvent.externaltimestamp, 4);
	dest = static_cast<void *>(static_cast<uint8_t *>(dest) + 4);

	uint32_t zeroPad = 0;

	std::memcpy(dest, &zeroPad, 4);
	dest = static_cast<void *>(static_cast<uint8_t *>(dest) + 4);

	uint64_t firstItem = (0x1 << 30) | ((anEvent.moduleid&0x3F) << 24) | ((anEvent.trigflag&0x1) << 23) 
		                 | ((anEvent.ch&0x1F) << 18) | (anEvent.pileup << 17) | (anEvent.overflow << 16)
										 | (anEvent.adc&0xFFFF);

	std::memcpy(dest, &firstItem, 4);
	dest = static_cast<void *>(static_cast<uint8_t *>(dest) + 4);

	std::memcpy(dest, &zeroPad, 4);
	dest = static_cast<void *>(static_cast<uint8_t *>(dest) + 4);

	uint64_t rolloverItem = (0x2 << 30) | (anEvent.rollovercounter&0x3FFFFFFF);

	std::memcpy(dest, &rolloverItem, 4);
	dest = static_cast<void *>(static_cast<uint8_t *>(dest) + 4);

	uint64_t secondItem = (0x3 << 30) | (anEvent.timestamp&0x3FFFFFFF);

	std::memcpy(dest, &secondItem, 4);
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

void send(CDataSink &sink, CRingItem &item)
{
	std::unique_ptr<CRingItem> pItem(&item);

	sink.putItem(*pItem);
}

void updateTimestamps(MDPPSCPSRO &anEvent)
{
	prevTimestamp_ns = timestamp_ns;
			timestamp_ns = getTimestamp_ns(anEvent);
	double timestampDiff_ns = timestamp_ns - prevTimestamp_ns;

	if (timestampDiff_ns < 0) {
		while (timestampDiff_ns < 0) {
			externalTimestampRolloverCounter += 1;
			timestamp_ns = getTimestamp_ns(anEvent);
			timestampDiff_ns = timestamp_ns - prevTimestamp_ns;
		}
	}

	prevMdppTimestamp_ns = mdppTimestamp_ns;
			mdppTimestamp_ns = getMdppTimestamp_ns(anEvent);
	double mdppTimestampDiff_ns = mdppTimestamp_ns - prevMdppTimestamp_ns;

	if (mdppTimestampDiff_ns < 0) {
		uint64_t rolloverCounter = timestampDiff_ns/MDPP_TIMESTAMP_MAX_NS;
		mdppRolloverCounter += rolloverCounter + 1;
		mdppTimestampDiff_ns += mdppRolloverCounter*MDPP_TIMESTAMP_MAX_NS;
	}

	anEvent.rollovercounter = mdppRolloverCounter;
	latestAbsoluteMdppTimestamp    = getAbsoluteMdppTimestamp(anEvent);
	latestAbsoluteMdppTimestamp_ns = getAbsoluteMdppTimestamp_ns(anEvent);
}

MDPPSCPSRO &getLastEvent()
{
	MDPPSCPSRO *pAnEvent = hitDeque.back();
	hitDeque.pop_back();

	return *pAnEvent;
}

MDPPSCPSRO &getFirstEvent()
{
	MDPPSCPSRO *pAnEvent = hitDeque.front();
	hitDeque.pop_front();

	return *pAnEvent;
}

MDPPSCPSRO &peekFirstEvent()
{
	return *hitDeque.front();
}

void collectEvent(MDPPSCPSRO &anEvent)
{
	eventQueue.push(&anEvent);
}

void sendCollection(CDataSink &sink)
{
	MDPPSCPSRO &anEvent = *eventQueue.front();

//	CPhysicsEventItem *pNewItem = new CPhysicsEventItem(anEvent.eventtimestamp, anEvent.sourceid, 0, 8192);
	CPhysicsEventItem *pNewItem = new CPhysicsEventItem();
	CPhysicsEventItem &newItem = *pNewItem;

	void *dest = newItem.getBodyCursor();

	uint16_t bodySize = 0xc*eventQueue.size() + 4; // an event(0xc)*#events + ender
	uint16_t vmusbHeader = ((anEvent.stackid&0x7) << 13) | (bodySize&0xFFF);

	std::memcpy(dest, &vmusbHeader, 2);
	dest = static_cast<void *>(static_cast<uint8_t *>(dest) + 2);

	while (!eventQueue.empty()) {
    std::unique_ptr<MDPPSCPSRO> pAnEvent(eventQueue.front());
		MDPPSCPSRO &anEvent = *pAnEvent;

		std::memcpy(dest, &anEvent.externaltimestamp, 4);
		dest = static_cast<void *>(static_cast<uint8_t *>(dest) + 4);

		uint32_t zeroPad = 0;

		std::memcpy(dest, &zeroPad, 4);
		dest = static_cast<void *>(static_cast<uint8_t *>(dest) + 4);

		uint64_t firstItem = (0x1 << 30) | ((anEvent.moduleid&0x3F) << 24) | ((anEvent.trigflag&0x1) << 23) 
			| ((anEvent.ch&0x1F) << 18) | (anEvent.pileup << 17) | (anEvent.overflow << 16)
			| (anEvent.adc&0xFFFF);

		std::memcpy(dest, &firstItem, 4);
		dest = static_cast<void *>(static_cast<uint8_t *>(dest) + 4);

		uint32_t timestampFromStart = getAbsoluteMdppTimestamp(anEvent) - windowStartTimestamp;

		std::memcpy(dest, &timestampFromStart, 4);
		dest = static_cast<void *>(static_cast<uint8_t *>(dest) + 4);

		uint64_t rolloverItem = (0x2 << 30) | (mdppRolloverCounter&0x3FFFFFFF);

		std::memcpy(dest, &rolloverItem, 4);
		dest = static_cast<void *>(static_cast<uint8_t *>(dest) + 4);

		uint64_t secondItem = (0x3 << 30) | (anEvent.timestamp&0x3FFFFFFF);

		std::memcpy(dest, &secondItem, 4);
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
}

void sending(CDataSink &sink, bool isTriggerChannel)
{
	if (isTriggerChannel && !dataCollecting) {
		MDPPSCPSRO &triggerEvent = getLastEvent();
		windowStartTimestamp    = getAbsoluteMdppTimestamp(triggerEvent) - windowStart;
		windowStartTimestamp_ns = getAbsoluteMdppTimestamp_ns(triggerEvent) - windowStart_ns;
		if (getAbsoluteMdppTimestamp(triggerEvent) < windowStart) {
			windowStartTimestamp    = 0;
			windowStartTimestamp_ns = 0;
		}
		windowEndTimestamp    = windowStartTimestamp + windowWidth;
		windowEndTimestamp_ns = windowStartTimestamp_ns + windowWidth_ns;

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
				cerr << "== This shouldn't be happening!" << endl;

				break;
			}
		}

#ifdef DEBUG
				cout << "== Collected trigger event ==" << endl;
				cout << "  MDPP timestamp from window start in ns: " << getAbsoluteMdppTimestamp_ns(triggerEvent) - windowStartTimestamp_ns << " (" << getAbsoluteMdppTimestamp(triggerEvent) - windowStartTimestamp << ")" << endl;
#endif

		collectEvent(triggerEvent);

		dataCollecting = true;
	} else if (dataCollecting) {
		MDPPSCPSRO &anEvent = peekFirstEvent();

		if (getAbsoluteMdppTimestamp(anEvent) >= windowStartTimestamp && getAbsoluteMdppTimestamp(anEvent) <= windowEndTimestamp)
		{
#ifdef DEBUG
				cout << "== Collected after trigger event ==" << endl;
				cout << "  MDPP timestamp from window start in ns: " << getAbsoluteMdppTimestamp_ns(anEvent) - windowStartTimestamp_ns << " (" << getAbsoluteMdppTimestamp(anEvent) - windowStartTimestamp << ")" << endl;
#endif
			anEvent = getFirstEvent();

			collectEvent(anEvent);
		}
		else if (windowEndTimestamp < latestAbsoluteMdppTimestamp)
		{
#ifdef DEBUG
				cout << "== Collecting done! Sending ==" << endl;
#endif
			dataCollecting = false;

			sendCollection(sink);
		}
		else
		{
			cerr << "== This shouldn't be happening! ==" << endl;
		}
	}	else {
		while (!hitDeque.empty()) {
			MDPPSCPSRO &anEvent = peekFirstEvent();

			if (latestAbsoluteMdppTimestamp - getAbsoluteMdppTimestamp(anEvent) > windowStart) {
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

void emptyingQueues(CDataSink &sink)
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
	}
	catch (CException& e) {
		std::cerr << "Failed to create data sink: ";
		usage(std::cerr, e.ReasonText(), argv[0]);
	}
	std::unique_ptr<CDataSink> sink(pSink);

	triggerChannel = atoi(argv[3]);
	windowStart_ns = atof(argv[4]);
	windowWidth_ns = atof(argv[5]);
	windowStart    = windowStart_ns*1000/MDPP_TDC_UNIT;
	windowWidth    = windowWidth_ns*1000/MDPP_TDC_UNIT;

	// The loop below consumes items from the ring buffer until
	// all are used up.  The use of an std::unique_ptr ensures that the
	// dynamically created ring items we get from the data source are
	// automatically deleted when we exit the block in which it's created.

	CRingItem *pItem;
	while ((pItem = pDataSource -> getItem() )) {
		CRingItem &item = *pItem;

		if (item.type() == PHYSICS_EVENT) {
			MDPPSCPSRO &anEvent = unpack(item);

			if (!timeSet && anEvent.ch >= 0) {
				uint64_t mdppTimestampRef = anEvent.timestamp;

				if (mdppTimestampRef < 41) {
					std::cerr << "I want at least 1ns in MDPP timestamp at first..." << std:: endl;
					std::cerr << "Seeing this message means the first hit in any channel is discarded." << std::endl;
					std::cerr << "Hope this not happen while it may not be that problematic...." << std::endl;

					std::unique_ptr<MDPPSCPSRO> pAnEvent(&anEvent);

					continue;
				}

				double timestampRef_ns = getTimestamp_ns(anEvent); // refDiff_ns = 0 at this point
				double mdppTimestampRef_ns = getMdppTimestamp_ns(anEvent);

				refDiff_ns = timestampRef_ns - mdppTimestampRef_ns;

				timeSet = true;
			}

			hitDeque.push_back(&anEvent);
			updateTimestamps(anEvent);
			sending(*sink, anEvent.ch == triggerChannel);
		} else if (item.type() == END_RUN) {
			emptyingQueues(*sink);
			send(*sink, item);
		} else {
			send(*sink, item);
		}
	}
	
	// We can only fall through here for file data sources... normal exit
	std::exit(EXIT_SUCCESS);
}
