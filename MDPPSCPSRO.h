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

#ifndef MDPPSCPSRO_H
#define MDPPSCPSRO_H

class MDPPSCPSRO {
	public:
		MDPPSCPSRO() {};
		~MDPPSCPSRO() {};

	public:
		uint64_t eventtimestamp;
		int sourceid;
		int stackid;
		int bodysize;
		uint64_t externaltimestamp;
		int moduleid;
		bool trigflag;
		int ch;
		bool pileup;
		bool overflow;
		unsigned int adc;
		unsigned long rollovercounter;
		unsigned long timestamp;
};

#endif
