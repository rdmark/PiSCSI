//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Copyright (C) 2014-2020 GIMONS
//  Copyright (C) akuker
//
//  Licensed under the BSD 3-Clause License. 
//  See LICENSE file in the project root folder.
//
//  [ RaSCSI Manager ]
//
//  Any meaningful logic should be included in this file so that it can be
//  tested using unit tests. The main "rascsi.cpp" file should contain very
//  little functional code.
//---------------------------------------------------------------------------
#pragma once

#include "os.h"
#include "scsi.h"
#include "fileio.h"
#include "disk.h"
#include "log.h"
#include "xm6.h"
#include "gpiobus.h"
#include "sasidev_ctrl.h"
#include <mutex>

class Rascsi_Manager{
    public:
        static Rascsi_Manager* GetInstance();
        void MapControler(FILE *fp, Disk **map);
        void AttachDevice(FILE *fp, Disk *disk, int id, int ui);
        void DetachDevice(FILE *fp, Disk *disk, int id, int ui);
        Disk* GetDevice(FILE *fp, int id, int ui);
        void ListDevice(FILE *fp);
        BOOL Init();
        void Close();
        void Reset();
        BOOL Step();

        static const int CtrlMax = 8;				// Maximum number of SCSI controllers
        static const int UnitNum=2;					// Number of units around controller


        // TODO: These need to be made private. All of the functionality that is using
        //       them directly should be updated to be issolated.
        // The Command_Thread class is WAAAAY too tightly coupled to this class.
        //
        static SASIDEV *m_ctrl[CtrlMax];			// Controller
        static Disk *m_disk[CtrlMax * UnitNum];		// Disk
        static GPIOBUS *m_bus;						// GPIO Bus


    private:
		static int m_actid;
    	static BUS::phase_t m_phase;
	    static BYTE m_data;
        static DWORD m_now;
        static Rascsi_Manager *m_instance;
        static BOOL m_active;
        static BOOL m_running;

        // Any PUBLIC functions should lock this before accessing the m_ctrl
        // m_disk or m_bus data structures. The Public functions could be
        // called from a different thread.
        static std::timed_mutex m_locked;
};
