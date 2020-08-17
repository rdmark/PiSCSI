#include "rascsi_mgr.h"
#include "log.h"
#include "sasidev_ctrl.h"
#include "scsidev_ctrl.h"

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
BOOL Rascsi_Manager::m_initialized = FALSE;

BOOL Rascsi_Manager::AttachDevice(FILE *fp, Disk *disk, int id, int unit_num){
	BOOL result;
	m_locked.lock();
	result = AttachDevicePrivate(fp, disk, id, unit_num);
	m_locked.unlock();
	return result;
}

BOOL Rascsi_Manager::DetachDevice(FILE *fp, int id, int unit_num){
	BOOL result;
	m_locked.lock();
	result = DetachDevicePrivate(fp, id, unit_num);
	m_locked.unlock();
	return result;
}

BOOL Rascsi_Manager::AttachDevicePrivate(FILE *fp, Disk *disk, int id, int unit_num)
{
	Rascsi_Device_Mode_e cur_mode = GetCurrentDeviceMode();
	int unitno = (id * UnitNum) + unit_num;

	if((disk->IsSASI() && (cur_mode == rascsi_device_scsi_mode)) ||
		(!disk->IsSCSI() && (cur_mode == rascsi_device_sasi_mode))){
			FPRT(fp, "Warning: Can't mix SASI and SCSI devices!\n");
			return FALSE;
	}

	// Check if something already exists at that SCSI ID / Unit Number
	if (m_disk[unitno]) {
		DetachDevicePrivate(fp,id,unit_num);
	}

	// Add the new unit
	m_disk[unitno] = disk;
	
	// If the controllder doesn't already exist, create it.
	if (m_ctrl[id] == nullptr){
		// We need to create a new controller
		if(disk->IsSASI()){
			m_ctrl[id] = new SASIDEV();
		}else{
			m_ctrl[id] = new SCSIDEV();
		}
		m_ctrl[id]->Connect(id, m_bus);
	}

	// Add the disk to the controller
	m_ctrl[id]->SetUnit(unit_num, disk);
	return TRUE;
}

BOOL Rascsi_Manager::DetachDevicePrivate(FILE *fp, int id, int unit_num)
{
	int unitno = (id * UnitNum) + unit_num;

	// Disconnect it from the controller
	if (m_ctrl[id]) {
		m_ctrl[id]->SetUnit(unit_num, NULL);
		// Free the Unit
		delete m_disk[unitno];
		m_disk[unitno] = nullptr;
	}else{
		fprintf(fp, "Warning: A controller was not connected to the drive at id %d un %d\n",id, unit_num);
		fprintf(fp, "This is some sort of internal error, since you can't have a device without a controller.\n");
		return FALSE;
	}

	// If there are no longer any units connected to this controller, delete it
	if(!m_ctrl[id]->HasUnit()){
		delete m_ctrl[id];
		m_ctrl[id] = nullptr;
	}

	return TRUE;
}

Disk* Rascsi_Manager::GetDevice(FILE *fp, int id, int unit_num)
{
	int unitno = (id * UnitNum) + unit_num;
	return m_disk[unitno];
}

//---------------------------------------------------------------------------
//
//	GetCurrentDeviceMode
//
//  Loop through all of the controllers and check if we have SCSI or SASI
//  controllers already created. (Note: We can't have a mix of them)
//
//---------------------------------------------------------------------------
Rascsi_Device_Mode_e Rascsi_Manager::GetCurrentDeviceMode(){

	Rascsi_Device_Mode_e mode = rascsi_device_unknown_mode;
	for(int i =0; i < CtrlMax; i++){
		if(m_ctrl[i] != nullptr){	
			if(m_ctrl[i]->IsSASI()){
				if(mode == rascsi_device_unknown_mode){
					mode = rascsi_device_sasi_mode;
				}else if(mode == rascsi_device_scsi_mode){
					mode = rascsi_device_invalid_mode;
					printf("Error: Mix of SCSI and SASI devices. This isn't allowed. Device %d was SASI, but expecting SCSI\n", i);
				}
			}else { // This is SCSI
				if(mode == rascsi_device_unknown_mode){
					mode = rascsi_device_scsi_mode;
				}else if(mode == rascsi_device_sasi_mode){
					mode = rascsi_device_invalid_mode;
					printf("Error: Mix of SCSI and SASI devices. This isn't allowed. Device %d was SCSI, but expecting SASI\n", i);
				}
			}
		}
	}
	return mode;
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

    if(m_initialized != FALSE){
        printf("Can not initialize Rascsi_Manager twice!!!\n");
        return FALSE;    
    }
	m_initialized = TRUE;

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

	m_running = TRUE;

	return TRUE;
}



BOOL Rascsi_Manager::Step()
{

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
			return FALSE;
		}

	m_locked.lock();
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


void Rascsi_Manager::Stop(){
	m_running = FALSE;
}
BOOL Rascsi_Manager::IsRunning(){
	return (m_running == TRUE);
}
