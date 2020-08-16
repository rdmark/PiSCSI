#include "rascsi_mgr.h"
#include "log.h"
#include "sasidev_ctrl.h"
#include "scsidev_ctrl.h"

Rascsi_Manager *Rascsi_Manager::m_instance = (Rascsi_Manager*)nullptr;
        

SASIDEV *Rascsi_Manager::m_ctrl[CtrlMax];			// Controller
Disk *Rascsi_Manager::m_disk[CtrlMax * UnitNum];		// Disk
GPIOBUS *Rascsi_Manager::m_bus;

int Rascsi_Manager::m_actid = -1;
BUS::phase_t Rascsi_Manager::m_phase = BUS::busfree;
BYTE Rascsi_Manager::m_data = 0;
DWORD Rascsi_Manager::m_now = 0;
BOOL Rascsi_Manager::m_active = FALSE;
BOOL Rascsi_Manager::m_running = FALSE;
std::timed_mutex Rascsi_Manager::m_locked;

Rascsi_Manager* Rascsi_Manager::GetInstance(){
    if(m_instance != nullptr){
        m_instance = new Rascsi_Manager();
    }
	return m_instance;
}


void Rascsi_Manager::AttachDevice(FILE *fp, Disk *disk, int id, int unit_num){
	int unitno = (id * UnitNum) + unit_num;

	// Get the lock
	m_locked.lock();

	// Check if something already exists at that SCSI ID / Unit Number
	if (m_disk[unitno]) {
		// Disconnect it from the controller
		if (m_ctrl[i]) {
			m_ctrl[i]->SetUnit(j, NULL);
		}else{
			printf("Warning: A controller was not connected to the drive at id %d un %d\n",id, unit_num);
		}

		// Free the Unit
		delete m_disk[unitno];
	}

	// Add the new unit
	m_disk[unitno] = disk;

	// Reconfigure all of the controllers
	for (i = 0; i < CtrlMax; i++) {
		// Examine the unit configuration
		sasi_num = 0;
		scsi_num = 0;
		for (j = 0; j < UnitNum; j++) {
			unitno = i * UnitNum + j;
			// branch by unit type
			if (m_disk[unitno]) {
				if (m_disk[unitno]->IsSASI()) {
					// Drive is SASI, so increment SASI count
					sasi_num++;
				} else {
					// Drive is SCSI, so increment SCSI count
					scsi_num++;
				}
			}

			// Remove the unit
			if (m_ctrl[i]) {
				m_ctrl[i]->SetUnit(j, NULL);
			}
		}

		// If there are no units connected
		if (sasi_num == 0 && scsi_num == 0) {
			if (m_ctrl[i]) {
				delete m_ctrl[i];
				m_ctrl[i] = NULL;
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
			if (m_ctrl[i] && !m_ctrl[i]->IsSASI()) {
				delete m_ctrl[i];
				m_ctrl[i] = NULL;
			}

			// Create a new SASI controller
			if (!m_ctrl[i]) {
				m_ctrl[i] = new SASIDEV();
				m_ctrl[i]->Connect(i, m_bus);
			}
		} else {
			// Only SCSI Unit(s)

			// Release the controller if it is not SCSI
			if (m_ctrl[i] && !m_ctrl[i]->IsSCSI()) {
				delete m_ctrl[i];
				m_ctrl[i] = NULL;
			}

			// Create a new SCSI controller
			if (!m_ctrl[i]) {
				m_ctrl[i] = new SCSIDEV();
				m_ctrl[i]->Connect(i, m_bus);
			}
		}

		// connect all units
		for (j = 0; j < UnitNum; j++) {
			unitno = i * UnitNum + j;
			if (m_disk[unitno]) {
				// Add the unit connection
				m_ctrl[i]->SetUnit(j, m_disk[unitno]);
			}
		}
	}



		}
        void Rascsi_Manager::DetachDevice(FILE *fp, Disk *map, int id, int ui)
		{
			return;
		}
        Disk* Rascsi_Manager::GetDevice(FILE *fp, int id, int ui)
		{
			return nullptr;

		}



//---------------------------------------------------------------------------
//
//	Controller Mapping
//
//---------------------------------------------------------------------------
void Rascsi_Manager::MapControler(FILE *fp, Disk **map)
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
			if (m_disk[unitno] != map[unitno]) {
				// Check if the original unit exists
				if (m_disk[unitno]) {
					// Disconnect it from the controller
					if (m_ctrl[i]) {
						m_ctrl[i]->SetUnit(j, NULL);
					}

					// Free the Unit
					delete m_disk[unitno];
				}

				// Setup a new unit
				m_disk[unitno] = map[unitno];
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
			if (m_disk[unitno]) {
				if (m_disk[unitno]->IsSASI()) {
					// Drive is SASI, so increment SASI count
					sasi_num++;
				} else {
					// Drive is SCSI, so increment SCSI count
					scsi_num++;
				}
			}

			// Remove the unit
			if (m_ctrl[i]) {
				m_ctrl[i]->SetUnit(j, NULL);
			}
		}

		// If there are no units connected
		if (sasi_num == 0 && scsi_num == 0) {
			if (m_ctrl[i]) {
				delete m_ctrl[i];
				m_ctrl[i] = NULL;
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
			if (m_ctrl[i] && !m_ctrl[i]->IsSASI()) {
				delete m_ctrl[i];
				m_ctrl[i] = NULL;
			}

			// Create a new SASI controller
			if (!m_ctrl[i]) {
				m_ctrl[i] = new SASIDEV();
				m_ctrl[i]->Connect(i, m_bus);
			}
		} else {
			// Only SCSI Unit(s)

			// Release the controller if it is not SCSI
			if (m_ctrl[i] && !m_ctrl[i]->IsSCSI()) {
				delete m_ctrl[i];
				m_ctrl[i] = NULL;
			}

			// Create a new SCSI controller
			if (!m_ctrl[i]) {
				m_ctrl[i] = new SCSIDEV();
				m_ctrl[i]->Connect(i, m_bus);
			}
		}

		// connect all units
		for (j = 0; j < UnitNum; j++) {
			unitno = i * UnitNum + j;
			if (m_disk[unitno]) {
				// Add the unit connection
				m_ctrl[i]->SetUnit(j, m_disk[unitno]);
			}
		}
	}
}


//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void Rascsi_Manager::Reset()
{
	int i;

	// Reset all of the controllers
	for (i = 0; i < CtrlMax; i++) {
		if (m_ctrl[i]) {
			m_ctrl[i]->Reset();
		}
	}

	// Reset the bus
	m_bus->Reset();
}

//---------------------------------------------------------------------------
//
//	List Devices
//
//---------------------------------------------------------------------------
void Rascsi_Manager::ListDevice(FILE *fp)
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
		pUnit = m_disk[i];

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

void Rascsi_Manager::Close(){
    	// Delete the disks
	for (int i = 0; i < (CtrlMax * UnitNum); i++) {
		if (m_disk[i]) {
			delete m_disk[i];
			m_disk[i] = NULL;
		}
	}

	// Delete the Controllers
	for (int i = 0; i < CtrlMax; i++) {
		if (m_ctrl[i]) {
			delete m_ctrl[i];
			m_ctrl[i] = NULL;
		}
	}

	// Cleanup the Bus
	m_bus->Cleanup();

	// Discard the GPIOBUS object
	delete m_bus;
}

BOOL Rascsi_Manager::Init(){

    if(m_instance != nullptr){
        printf("Can not initialize Rascsi_Manager twice!!!\n");
        return FALSE;    
    }

    m_instance = new Rascsi_Manager();

	// GPIOBUS creation
	m_bus = new GPIOBUS();

	// GPIO Initialization
	if (!m_bus->Init()) {
		return FALSE;
	}

	// Bus Reset
	m_bus->Reset();

	// Controller initialization
	for (int i = 0; i < CtrlMax; i++) {
		m_ctrl[i] = NULL;
	}

	// Disk Initialization
	for (int i = 0; i < CtrlMax; i++) {
		m_disk[i] = NULL;
	}


	// Reset
	Reset();

	return TRUE;
}



BOOL Rascsi_Manager::Step()
{

	m_locked.lock();
		// Work initialization
		m_actid = -1;
		m_phase = BUS::busfree;

#ifdef USE_SEL_EVENT_ENABLE
		// SEL signal polling
		if (m_bus->PollSelectEvent() < 0) {
			// Stop on interrupt
			if (errno == EINTR) {
				m_locked.unlock();
				return FALSE;
			}
			m_locked.unlock();
			return FALSE;
		}

		// Get the bus
		m_bus->Aquire();
#else
		m_bus->Aquire();
		if (!m_bus->GetSEL()) {
#if !defined(BAREMETAL)
			usleep(0);
#endif	// !BAREMETAL
			m_locked.unlock();
			return FALSE;
		}
#endif	// USE_SEL_EVENT_ENABLE

        // Wait until BSY is released as there is a possibility for the
        // initiator to assert it while setting the ID (for up to 3 seconds)
		if (m_bus->GetBSY()) {
			m_now = SysTimer::GetTimerLow();
			while ((SysTimer::GetTimerLow() - m_now) < 3 * 1000 * 1000) {
				m_bus->Aquire();
				if (!m_bus->GetBSY()) {
					break;
				}
			}
		}

		// Stop because it the bus is busy or another device responded
		if (m_bus->GetBSY() || !m_bus->GetSEL()) {
			m_locked.unlock();
			return FALSE;
		}

		// Notify all controllers
		m_data = m_bus->GetDAT();
		for (int i = 0; i < CtrlMax; i++) {
			if (!m_ctrl[i] || (m_data & (1 << i)) == 0) {
				continue;
			}

			// Find the target that has moved to the selection phase
			if (m_ctrl[i]->Process() == BUS::selection) {
				// Get the target ID
				m_actid = i;

				// Bus Selection phase
				m_phase = BUS::selection;
				break;
			}
		}

		// Return to bus monitoring if the selection phase has not started
		if (m_phase != BUS::selection) {
			m_locked.unlock();
			return FALSE;
		}

		// Start target device
		m_active = TRUE;

#if !defined(USE_SEL_EVENT_ENABLE) && !defined(BAREMETAL)
		// Scheduling policy setting (highest priority)
		schparam.sched_priority = sched_get_priority_max(SCHED_FIFO);
		sched_setscheduler(0, SCHED_FIFO, &schparam);
#endif	// !USE_SEL_EVENT_ENABLE && !BAREMETAL

		// Loop until the bus is free
		while (m_running) {
			// Target drive
			m_phase = m_ctrl[m_actid]->Process();

			// End when the bus is free
			if (m_phase == BUS::busfree) {
				break;
			}
		}

#if !defined(USE_SEL_EVENT_ENABLE) && !defined(BAREMETAL)
		// Set the scheduling priority back to normal
		schparam.sched_priority = 0;
		sched_setscheduler(0, SCHED_OTHER, &schparam);
#endif	// !USE_SEL_EVENT_ENABLE && !BAREMETAL

		// End the target travel
		m_active = FALSE;

	m_locked.unlock();
	return TRUE;

}
