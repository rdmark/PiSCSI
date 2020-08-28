//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
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

//---------------------------------------------------------------------------
//
//  Constant declarations
//
//---------------------------------------------------------------------------
#define CtrlMax	8					// Maximum number of SCSI controllers
#define UnitNum	2					// Number of units around controller
#ifdef BAREMETAL
#define FPRT(fp, ...) printf( __VA_ARGS__ )
#else
#define FPRT(fp, ...) fprintf(fp, __VA_ARGS__ )
#endif	// BAREMETAL

//---------------------------------------------------------------------------
//
//	Variable declarations
//
//---------------------------------------------------------------------------
static volatile BOOL running;		// Running flag
static volatile BOOL active;		// Processing flag
SASIDEV *ctrl[CtrlMax];				// Controller
Disk *disk[CtrlMax * UnitNum];		// Disk
GPIOBUS *bus;						// GPIO Bus
#ifdef BAREMETAL
FATFS fatfs;						// FatFS
#else
#endif	// BAREMETAL

#ifndef BAREMETAL
//---------------------------------------------------------------------------
//
//	Signal Processing
//
//---------------------------------------------------------------------------
void KillHandler(int sig)
{
	// Stop instruction
	running = FALSE;
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
	int i;

#ifndef BAREMETAL

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
#endif // BAREMETAL

	// GPIOBUS creation
	bus = new GPIOBUS();

	// GPIO Initialization
	if (!bus->Init()) {
		return FALSE;
	}

	// Bus Reset
	bus->Reset();

	// Controller initialization
	for (i = 0; i < CtrlMax; i++) {
		ctrl[i] = NULL;
	}

	// Disk Initialization
	for (i = 0; i < CtrlMax; i++) {
		disk[i] = NULL;
	}

	// Other
	running = FALSE;
	active = FALSE;

	printf("Command_Thread Init\n");
	if(!Command_Thread::Init()){
	printf("\tCommand_Thread FAILED!!\n");
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
	int i;

	// Delete the disks
	for (i = 0; i < CtrlMax * UnitNum; i++) {
		if (disk[i]) {
			delete disk[i];
			disk[i] = NULL;
		}
	}

	// Delete the Controllers
	for (i = 0; i < CtrlMax; i++) {
		if (ctrl[i]) {
			delete ctrl[i];
			ctrl[i] = NULL;
		}
	}

	// Cleanup the Bus
	bus->Cleanup();

	// Discard the GPIOBUS object
	delete bus;

#ifndef BAREMETAL
	// Close the command socket
	Command_Thread::Close();
#endif // BAREMETAL
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void Reset()
{
	int i;

	// Reset all of the controllers
	for (i = 0; i < CtrlMax; i++) {
		if (ctrl[i]) {
			ctrl[i]->Reset();
		}
	}

	// Reset the bus
	bus->Reset();
}

//---------------------------------------------------------------------------
//
//	List Devices
//
//---------------------------------------------------------------------------
void ListDevice(FILE *fp)
{
	int i;
	int id;
	int un;
	Disk *pUnit;
	Filepath filepath;
	BOOL find;
	char type[5];

	find = FALSE;
	type[4] = 0;
	for (i = 0; i < CtrlMax * UnitNum; i++) {
		// Initialize ID and unit number
		id = i / UnitNum;
		un = i % UnitNum;
		pUnit = disk[i];

		// skip if unit does not exist or null disk
		if (pUnit == NULL || pUnit->IsNULL()) {
			continue;
		}

		// Output the header
        if (!find) {
			FPRT(fp, "\n");
			FPRT(fp, "+----+----+------+-------------------------------------\n");
			FPRT(fp, "| ID | UN | TYPE | DEVICE STATUS\n");
			FPRT(fp, "+----+----+------+-------------------------------------\n");
			find = TRUE;
		}

		// ID,UNIT,Type,Device Status
		type[0] = (char)(pUnit->GetID() >> 24);
		type[1] = (char)(pUnit->GetID() >> 16);
		type[2] = (char)(pUnit->GetID() >> 8);
		type[3] = (char)(pUnit->GetID());
		FPRT(fp, "|  %d |  %d | %s | ", id, un, type);

		// mount status output
		if (pUnit->GetID() == MAKEID('S', 'C', 'B', 'R')) {
			FPRT(fp, "%s", "HOST BRIDGE");
		} else {
			pUnit->GetPath(filepath);
			FPRT(fp, "%s",
				(pUnit->IsRemovable() && !pUnit->IsReady()) ?
				"NO MEDIA" : filepath.GetPath());
		}

		// Write protection status
		if (pUnit->IsRemovable() && pUnit->IsReady() && pUnit->IsWriteP()) {
			FPRT(fp, "(WRITEPROTECT)");
		}

		// Goto the next line
		FPRT(fp, "\n");
	}

	// If there is no controller, find will be null
	if (!find) {
		FPRT(fp, "No device is installed.\n");
		return;
	}

	FPRT(fp, "+----+----+------+-------------------------------------\n");
}

//---------------------------------------------------------------------------
//
//	Controller Mapping
//
//---------------------------------------------------------------------------
void MapControler(FILE *fp, Disk **map)
{
	int i;
	int j;
	int unitno;
	int sasi_num;
	int scsi_num;

	// Replace the changed unit
	for (i = 0; i < CtrlMax; i++) {
		for (j = 0; j < UnitNum; j++) {
			unitno = i * UnitNum + j;
			if (disk[unitno] != map[unitno]) {
				// Check if the original unit exists
				if (disk[unitno]) {
					// Disconnect it from the controller
					if (ctrl[i]) {
						ctrl[i]->SetUnit(j, NULL);
					}

					// Free the Unit
					delete disk[unitno];
				}

				// Setup a new unit
				disk[unitno] = map[unitno];
			}
		}
	}

	// Reconfigure all of the controllers
	for (i = 0; i < CtrlMax; i++) {
		// Examine the unit configuration
		sasi_num = 0;
		scsi_num = 0;
		for (j = 0; j < UnitNum; j++) {
			unitno = i * UnitNum + j;
			// branch by unit type
			if (disk[unitno]) {
				if (disk[unitno]->IsSASI()) {
					// Drive is SASI, so increment SASI count
					sasi_num++;
				} else {
					// Drive is SCSI, so increment SCSI count
					scsi_num++;
				}
			}

			// Remove the unit
			if (ctrl[i]) {
				ctrl[i]->SetUnit(j, NULL);
			}
		}

		// If there are no units connected
		if (sasi_num == 0 && scsi_num == 0) {
			if (ctrl[i]) {
				delete ctrl[i];
				ctrl[i] = NULL;
				continue;
			}
		}

		// Mixture of SCSI and SASI
		if (sasi_num > 0 && scsi_num > 0) {
			FPRT(fp, "Error : SASI and SCSI can't be mixed\n");
			continue;
		}

		if (sasi_num > 0) {
			// Only SASI Unit(s)

			// Release the controller if it is not SASI
			if (ctrl[i] && !ctrl[i]->IsSASI()) {
				delete ctrl[i];
				ctrl[i] = NULL;
			}

			// Create a new SASI controller
			if (!ctrl[i]) {
				ctrl[i] = new SASIDEV();
				ctrl[i]->Connect(i, bus);
			}
		} else {
			// Only SCSI Unit(s)

			// Release the controller if it is not SCSI
			if (ctrl[i] && !ctrl[i]->IsSCSI()) {
				delete ctrl[i];
				ctrl[i] = NULL;
			}

			// Create a new SCSI controller
			if (!ctrl[i]) {
				ctrl[i] = new SCSIDEV();
				ctrl[i]->Connect(i, bus);
			}
		}

		// connect all units
		for (j = 0; j < UnitNum; j++) {
			unitno = i * UnitNum + j;
			if (disk[unitno]) {
				// Add the unit connection
				ctrl[i]->SetUnit(j, disk[unitno]);
			}
		}
	}
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
#endif	// BAREMETAL
	int i;
	int ret;
	int actid;
	DWORD now;
	BUS::phase_t phase;
	BYTE data;
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

	// Reset
	Reset();

#ifdef BAREMETAL
	// BUSY assert (to hold the host side)
	bus->SetBSY(TRUE);

	// Argument parsing
	if (!Command_Thread::ParseConfig(argc, argv)) {
		ret = EINVAL;
		goto err_exit;
	}

	// Release the busy signal
	bus->SetBSY(FALSE);
#endif

#ifndef BAREMETAL
    // Set the affinity to a specific processor core
	FixCpu(3);

#if defined(USE_SEL_EVENT_ENABLE) && defined(__linux__)
	// Scheduling policy setting (highest priority)
	schparam.sched_priority = sched_get_priority_max(SCHED_FIFO);
	sched_setscheduler(0, SCHED_FIFO, &schparam);
#endif	// USE_SEL_EVENT_ENABLE
#endif	// BAREMETAL

	// Start execution
	running = TRUE;

	// Main Loop
	while (running) {
		// Work initialization
		actid = -1;
		phase = BUS::busfree;

#ifdef USE_SEL_EVENT_ENABLE
		// SEL signal polling
		if (bus->PollSelectEvent() < 0) {
			// Stop on interrupt
			if (errno == EINTR) {
				break;
			}
			continue;
		}

		// Get the bus
		bus->Aquire();
#else
		bus->Aquire();
		if (!bus->GetSEL()) {
#if !defined(BAREMETAL)
			usleep(0);
#endif	// !BAREMETAL
			continue;
		}
#endif	// USE_SEL_EVENT_ENABLE

        // Wait until BSY is released as there is a possibility for the
        // initiator to assert it while setting the ID (for up to 3 seconds)
		if (bus->GetBSY()) {
			now = SysTimer::GetTimerLow();
			while ((SysTimer::GetTimerLow() - now) < 3 * 1000 * 1000) {
				bus->Aquire();
				if (!bus->GetBSY()) {
					break;
				}
			}
		}

		// Stop because it the bus is busy or another device responded
		if (bus->GetBSY() || !bus->GetSEL()) {
			continue;
		}

		// Notify all controllers
		data = bus->GetDAT();
		for (i = 0; i < CtrlMax; i++) {
			if (!ctrl[i] || (data & (1 << i)) == 0) {
				continue;
			}

			// Find the target that has moved to the selection phase
			if (ctrl[i]->Process() == BUS::selection) {
				// Get the target ID
				actid = i;

				// Bus Selection phase
				phase = BUS::selection;
				break;
			}
		}

		// Return to bus monitoring if the selection phase has not started
		if (phase != BUS::selection) {
			continue;
		}

		// Start target device
		active = TRUE;

#if !defined(USE_SEL_EVENT_ENABLE) && !defined(BAREMETAL)
		// Scheduling policy setting (highest priority)
		schparam.sched_priority = sched_get_priority_max(SCHED_FIFO);
		sched_setscheduler(0, SCHED_FIFO, &schparam);
#endif	// !USE_SEL_EVENT_ENABLE && !BAREMETAL

		// Loop until the bus is free
		while (running) {
			// Target drive
			phase = ctrl[actid]->Process();

			// End when the bus is free
			if (phase == BUS::busfree) {
				break;
			}
		}

#if !defined(USE_SEL_EVENT_ENABLE) && !defined(BAREMETAL)
		// Set the scheduling priority back to normal
		schparam.sched_priority = 0;
		sched_setscheduler(0, SCHED_OTHER, &schparam);
#endif	// !USE_SEL_EVENT_ENABLE && !BAREMETAL

		// End the target travel
		active = FALSE;
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
