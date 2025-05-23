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
		int sourceid;
		int stackid;
		int bodysize;
		int moduleid;
		int tdcresolution;
		int ch;
		bool pileup;
		bool overflow;
		uint32_t adc;
		uint64_t rollovercounter;
		uint64_t timestamp;
};

#endif
