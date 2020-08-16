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

enum Rascsi_Device_Mode_e{
    rascsi_device_unknown_mode,
    rascsi_device_sasi_mode,
    rascsi_device_scsi_mode,
    rascsi_device_invalid_mode,
};

class Rascsi_Manager{
    public:
        static Rascsi_Manager* GetInstance();
        static void MapControler(FILE *fp, Disk **map);

        static void Stop();
        static BOOL IsRunning();

        static BOOL AttachDevice(FILE *fp, Disk *disk, int id, int ui);
        static BOOL DetachDevice(FILE *fp, int id, int ui);
        static Disk* GetDevice(FILE *fp, int id, int ui);
        static void ListDevice(FILE *fp);
        static BOOL Init();
        static void Close();
        static void Reset();
        static BOOL Step();
        static Rascsi_Device_Mode_e GetCurrentDeviceMode();

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
        static BOOL m_active;
        static BOOL m_running;
        static BOOL m_initialized;

        static BOOL AttachDevicePrivate(FILE *fp, Disk *disk, int id, int ui);
        static BOOL DetachDevicePrivate(FILE *fp, int id, int ui);

        // Any PUBLIC functions should lock this before accessing the m_ctrl
        // m_disk or m_bus data structures. The Public functions could be
        // called from a different thread.
        static std::timed_mutex m_locked;
};
