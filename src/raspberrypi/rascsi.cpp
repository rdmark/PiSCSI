//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//  Copyright (C) akuker
// 
//	[ RaSCSI main ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "filepath.h"
#include "fileio.h"
#include "devices/disk.h"
#include "devices/sasihd.h"
#include "devices/scsihd.h"
#include "devices/scsihd_apple.h"
#include "devices/scsihd_nec.h"
#include "devices/scsicd.h"
#include "devices/scsimo.h"
#include "devices/scsi_host_bridge.h"
#include "controllers/scsidev_ctrl.h"
#include "controllers/sasidev_ctrl.h"
#include "gpiobus.h"
#include "command_thread.h"
#include "rascsi_mgr.h"

#include "sasidev_ctrl.h"
#include "scsidev_ctrl.h"

#include "sasihd.h"
#include "scsihd.h"
#include "scsihd_apple.h"
#include "scsihd_nec.h"
#include "scsicd.h"
#include "scsimo.h"
#include "scsi_host_bridge.h"



//---------------------------------------------------------------------------
//
//  Constant declarations
//
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//
//	Variable declarations
//
//---------------------------------------------------------------------------
#ifdef BAREMETAL
FATFS fatfs;						// FatFS
#else
#endif	// BAREMETAL

#ifndef CONNECT_DESC
#define CONNECT_DESC "UNKNOWN"
#endif

#ifndef BAREMETAL
//---------------------------------------------------------------------------
//
//	Signal Processing
//
//---------------------------------------------------------------------------
void KillHandler(int sig)
{
	// Stop instruction
	Rascsi_Manager::Stop();
}
#endif	// BAREMETAL

//---------------------------------------------------------------------------
//
//	Banner Output
//
//---------------------------------------------------------------------------
void Banner(int argc, char* argv[])
{
	FPRT(stdout,"SCSI Target Emulator RaSCSI(*^..^*)\n");
	FPRT(stdout,"  68k MLA Edition\n");
	FPRT(stdout,"Forked from GIMONS version %01d.%01d%01d(%s, %s)\n",
		(int)((VERSION >> 8) & 0xf),
		(int)((VERSION >> 4) & 0xf),
		(int)((VERSION     ) & 0xf),
		__DATE__,
		__TIME__);
	FPRT(stdout,"Powered by XM6 TypeG Technology / ");
	FPRT(stdout,"Copyright (C) 2016-2020 GIMONS\n");
	FPRT(stdout,"Connect type : %s\n", CONNECT_DESC);
	FPRT(stdout,"Build on %s at %s\n", __DATE__, __TIME__);

	if ((argc > 1 && strcmp(argv[1], "-h") == 0) ||
		(argc > 1 && strcmp(argv[1], "--help") == 0)){
		FPRT(stdout,"\n");
		FPRT(stdout,"Usage: %s [-IDn FILE] ...\n\n", argv[0]);
		FPRT(stdout," n is SCSI identification number(0-7).\n");
		FPRT(stdout," FILE is disk image file.\n\n");
		FPRT(stdout,"Usage: %s [-HDn FILE] ...\n\n", argv[0]);
		FPRT(stdout," n is X68000 SASI HD number(0-15).\n");
		FPRT(stdout," FILE is disk image file.\n\n");
		FPRT(stdout," Image type is detected based on file extension.\n");
		FPRT(stdout,"  hdf : SASI HD image(XM6 SASI HD image)\n");
		FPRT(stdout,"  hds : SCSI HD image(XM6 SCSI HD image)\n");
		FPRT(stdout,"  hdn : SCSI HD image(NEC GENUINE)\n");
		FPRT(stdout,"  hdi : SCSI HD image(Anex86 HD image)\n");
		FPRT(stdout,"  nhd : SCSI HD image(T98Next HD image)\n");
		FPRT(stdout,"  hda : SCSI HD image(APPLE GENUINE)\n");
		FPRT(stdout,"  mos : SCSI MO image(XM6 SCSI MO image)\n");
		FPRT(stdout,"  iso : SCSI CD image(ISO 9660 image)\n");

#ifndef BAREMETAL
		exit(0);
#endif	// BAREMETAL
	}
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL Init()
{
	printf("Rascsi_Manager Init\n");
	if(!Rascsi_Manager::Init()){
	printf("\tRascsi_Manager FAILED!!\n");
		return FALSE;
	}

	printf("Command_Thread Init\n");
	if(!Command_Thread::Init()){
	printf("\tCommand_Thread FAILED!!\n");
		return FALSE;
	}

	// Interrupt handler settings
	if (signal(SIGINT, KillHandler) == SIG_ERR) {
		return FALSE;
	}
	if (signal(SIGHUP, KillHandler) == SIG_ERR) {
		return FALSE;
	}
	if (signal(SIGTERM, KillHandler) == SIG_ERR) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void Cleanup()
{
	Rascsi_Manager::Close();

#ifndef BAREMETAL
	Command_Thread::Close();
#endif // BAREMETAL
}

//---------------------------------------------------------------------------
//
//	Main processing
//
//---------------------------------------------------------------------------
#ifdef BAREMETAL
extern "C"
int startrascsi(void)
{
	int argc = 0;
	char** argv = NULL;
#else
int main(int argc, char* argv[])
{
	int i=0;
#endif	// BAREMETAL
	int ret;
#ifndef BAREMETAL
	struct sched_param schparam;
#endif	// BAREMETAL

	// Output the Banner
	Banner(argc, argv);

	// Initialize
	ret = 0;
	if (!Init()) {
		ret = EPERM;
		goto init_exit;
	}


#ifdef BAREMETAL
	// BUSY assert (to hold the host side)
	Rascsi_Manager::m_bus->SetBSY(TRUE);

	// Argument parsing
	if (!Command_Thread::ParseConfig(argc, argv)) {
		ret = EINVAL;
		goto err_exit;
	}
	// Release the busy signal
	Rascsi_Manager::m_bus->SetBSY(FALSE);

#endif

	// For non-baremetal versions, we won't process the startup arguments... yet
	printf("Here1\n");

#ifndef BAREMETAL
    // Set the affinity to a specific processor core
	FixCpu(3);

#if defined(USE_SEL_EVENT_ENABLE) && defined(__linux__)
	// Scheduling policy setting (highest priority)
	schparam.sched_priority = sched_get_priority_max(SCHED_FIFO);
	sched_setscheduler(0, SCHED_FIFO, &schparam);
#endif	// USE_SEL_EVENT_ENABLE
#endif	// BAREMETAL

	printf("entering main loop \n");
	// Main Loop
	while (Rascsi_Manager::IsRunning()) {
		printf("step %d\n", i++);
		Rascsi_Manager::Step();
	}
	printf("Exited main loop\n");

#ifdef BAREMETAL
err_exit:
#endif
	// Cleanup
	Cleanup();

init_exit:
#if !defined(BAREMETAL)
	exit(ret);
#else
	return ret;
#endif	// BAREMETAL
}
