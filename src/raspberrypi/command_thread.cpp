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
//  [ Command thread ]
//
//  This is a background thread that runs, listening for commands on port
//  6868 for commands from the rasctl utility. This also includes the logic
//  for handling the initialization commands that are specified at startup.
//
//---------------------------------------------------------------------------

#include "command_thread.h"

#include "os.h"
#include "rasctl_command.h"
#include "rascsi_mgr.h"
#include "sasihd.h"
#include "scsihd.h"
#include "scsihd_nec.h"
#include "scsihd_apple.h"
#include "scsicd.h"
#include "scsimo.h"
#include "scsi_host_bridge.h"

int Command_Thread::m_monsocket = -1;				// Monitor Socket
pthread_t Command_Thread::m_monthread;				// Monitor Thread

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL Command_Thread::Init()
{

#ifndef BAREMETAL
	struct sockaddr_in server;
	int yes;

	// Create socket for monitor
	m_monsocket = socket(PF_INET, SOCK_STREAM, 0);
	memset(&server, 0, sizeof(server));
	server.sin_family = PF_INET;
	server.sin_port   = htons(Rasctl_Command::PortNumber);
	server.sin_addr.s_addr = htonl(INADDR_ANY);

	// allow address reuse
	yes = 1;
	if (setsockopt(
		m_monsocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0){
		return FALSE;
	}

	// Bind
	if (bind(m_monsocket, (struct sockaddr *)&server,
		sizeof(struct sockaddr_in)) < 0) {
		FPRT(stderr, "Error : Already running?\n");
		return FALSE;
	}

	// Create Monitor Thread
	pthread_create(&m_monthread, NULL, MonThread, NULL);


#endif // BAREMETAL

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Close/Cleanup the command thread
//
//---------------------------------------------------------------------------
void Command_Thread::Close(){
	// Close the monitor socket
	if (m_monsocket >= 0) {
		close(m_monsocket);
	}
}


//---------------------------------------------------------------------------
//
//	Argument Parsing
//
//---------------------------------------------------------------------------
#ifdef BAREMETAL
BOOL ParseConfig(int argc, char* argv[])
{
	FRESULT fr;
	FIL fp;
	char line[512];
	int id;
	int un;
	int type;
	char *argID;
	char *argPath;
	int len;
	char *ext;

	// Mount the SD card
	fr = f_mount(&fatfs, "", 1);
	if (fr != FR_OK) {
		FPRT(stderr, "Error : SD card mount failed.\n");
		return FALSE;
	}

	// If there is no setting file, the processing is interrupted
	fr = f_open(&fp, "rascsi.ini", FA_READ);
	if (fr != FR_OK) {
		return FALSE;
	}

	// Start Decoding

	while (TRUE) {
		// Get one Line
		memset(line, 0x00, sizeof(line));
		if (f_gets(line, sizeof(line) -1, &fp) == NULL) {
			break;
		}

		// Delete the CR/LF
		len = strlen(line);
		while (len > 0) {
			if (line[len - 1] != '\r' && line[len - 1] != '\n') {
				break;
			}
			line[len - 1] = '\0';
			len--;
		}

		// Get the ID and Path
		argID = &line[0];
		argPath = &line[4];
		line[3] = '\0';

		// Check if the line is an empty string
		if (argID[0] == '\0' || argPath[0] == '\0') {
			continue;
		}

		if (strlen(argID) == 3 && xstrncasecmp(argID, "id", 2) == 0) {
			// ID or ID Format

			// Check that the ID number is valid (0-7)
			if (argID[2] < '0' || argID[2] > '7') {
				FPRT(stderr,
					"Error : Invalid argument(IDn n=0-7) [%c]\n", argID[2]);
				goto parse_error;
			}

			// The ID unit is good
            id = argID[2] - '0';
			un = 0;
		} else if (xstrncasecmp(argID, "hd", 2) == 0) {
			// HD or HD format

			if (strlen(argID) == 3) {
				// Check that the HD number is valid (0-9)
				if (argID[2] < '0' || argID[2] > '9') {
					FPRT(stderr,
						"Error : Invalid argument(HDn n=0-15) [%c]\n", argID[2]);
					goto parse_error;
				}

				// ID was confirmed
				id = (argID[2] - '0') / UnitNum;
				un = (argID[2] - '0') % UnitNum;
			} else if (strlen(argID) == 4) {
				// Check that the HD number is valid (10-15)
				if (argID[2] != '1' || argID[3] < '0' || argID[3] > '5') {
					FPRT(stderr,
						"Error : Invalid argument(HDn n=0-15) [%c]\n", argID[2]);
					goto parse_error;
				}

				// The ID unit is good - create the id and unit number
				id = ((argID[3] - '0') + 10) / UnitNum;
				un = ((argID[3] - '0') + 10) % UnitNum;
				argPath++;
			} else {
				FPRT(stderr,
					"Error : Invalid argument(IDn or HDn) [%s]\n", argID);
				goto parse_error;
			}
		} else {
			FPRT(stderr,
				"Error : Invalid argument(IDn or HDn) [%s]\n", argID);
			goto parse_error;
		}

		// Skip if there is already an active device
		if (disk[id * UnitNum + un] &&
			!disk[id * UnitNum + un]->IsNULL()) {
			continue;
		}

		// Initialize device type
		type = -1;

		// Check ethernet and host bridge
		if (xstrcasecmp(argPath, "bridge") == 0) {
			type = 4;
		} else {
			// Check the path length
			len = strlen(argPath);
			if (len < 5) {
				FPRT(stderr,
					"Error : Invalid argument(File path is short) [%s]\n",
					argPath);
				goto parse_error;
			}

			// Does the file have an extension?
			if (argPath[len - 4] != '.') {
				FPRT(stderr,
					"Error : Invalid argument(No extension) [%s]\n", argPath);
				goto parse_error;
			}

			// Figure out what the type is
			ext = &argPath[len - 3];
			if (xstrcasecmp(ext, "hdf") == 0 ||
				xstrcasecmp(ext, "hds") == 0 ||
				xstrcasecmp(ext, "hdn") == 0 ||
				xstrcasecmp(ext, "hdi") == 0 || xstrcasecmp(ext, "nhd") == 0 ||
				xstrcasecmp(ext, "hda") == 0) {
				// HD(SASI/SCSI)
				type = 0;
			} else if (strcasecmp(ext, "mos") == 0) {
				// MO
				type = 2;
			} else if (strcasecmp(ext, "iso") == 0) {
				// CD
				type = 3;
			} else {
				// Cannot determine the file type
				FPRT(stderr,
					"Error : Invalid argument(file type) [%s]\n", ext);
				goto parse_error;
			}
		}

		// Execute the command
		if (!ExecuteCommand(stderr, id, un, 0, type, argPath)) {
			goto parse_error;
		}
	}

	// Close the configuration file
	f_close(&fp);

	// Display the device list
	ListDevice(stdout);

	return TRUE;

parse_error:

	// Close the configuration file
	f_close(&fp);

	return FALSE;
}
#else
#endif  // BAREMETAL

//---------------------------------------------------------------------------
//
//	Monitor Thread
//
//---------------------------------------------------------------------------
void *Command_Thread::MonThread(void *param)
{
#ifndef BAREMETAL
	struct sched_param schedparam;
	struct sockaddr_in client;
	socklen_t len;
	int fd;
	FILE *fp;
	char buf[BUFSIZ];
	char *p;
	Rasctl_Command *new_command;


	// Scheduler Settings
	schedparam.sched_priority = 0;
#if defined(__linux__)
	// sched_setscheduler is only available on Linux
	sched_setscheduler(0, SCHED_IDLE, &schedparam);
#endif
	// Set the affinity to a specific processor core
	FixCpu(2);

	// Setup the monitor socket to receive commands
	listen(m_monsocket, 1);

	while (Rascsi_Manager::IsRunning()) {
		// Wait for connection
		memset(&client, 0, sizeof(client));
		len = sizeof(client);
		fd = accept(m_monsocket, (struct sockaddr*)&client, &len);
		if (fd < 0) {
			break;
		}

		// Fetch the command
		fp = fdopen(fd, "r+");
		p = fgets(buf, BUFSIZ, fp);

		// If we received a command....
		if (p) {

			new_command = Rasctl_Command::DeSerialize(p,BUFSIZ);

			if(new_command != nullptr){
				// Execute the command
				ExecuteCommand(fp, new_command);
			}

	}

		// Release the connection
		fclose(fp);
		close(fd);
	}
#endif	// BAREMETAL

	return NULL;
}


//---------------------------------------------------------------------------
//
//	Command Processing
//
//---------------------------------------------------------------------------
BOOL Command_Thread::ExecuteCommand(FILE *fp, Rasctl_Command *incoming_command)
{
	Filepath filepath;
	BOOL result = FALSE;

	switch(incoming_command->cmd){
		case rasctl_cmd_shutdown:
			result = DoShutdown(fp,incoming_command);
			break;
		case rasctl_cmd_list:
			result = DoList(fp,incoming_command);
		break;
		case rasctl_cmd_attach:
			result = DoAttach(fp,incoming_command);
			break;
		case rasctl_cmd_insert:
			result = DoInsert(fp,incoming_command);
			break;
		case rasctl_cmd_eject:
			result = DoEject(fp,incoming_command);
			break;
		case rasctl_cmd_protect:
			result = DoProtect(fp,incoming_command);
			break;
		case rasctl_cmd_detach:
			result = DoDetach(fp,incoming_command);
			break;
		default:
			fprintf(fp, "UNKNOWN COMMAND RECEIVED\n");
	}

	return result;
}

BOOL Command_Thread::DoShutdown(FILE *fp, Rasctl_Command *incoming_command){
	fprintf(fp, "Stopping the RaSCSI application\n");
	Rascsi_Manager::Stop();
	return TRUE;
}

BOOL Command_Thread::DoList(FILE *fp, Rasctl_Command *incoming_command){
	Rascsi_Manager::ListDevice(fp);
	return TRUE;
}

BOOL Command_Thread::DoAttach(FILE *fp, Rasctl_Command *incoming_command){
	Filepath filepath;
	Disk *pUnit = nullptr;

	// Create a new drive, based upon type
	switch (incoming_command->type) {
		case rasctl_dev_sasi_hd:		// HDF
			pUnit = new SASIHD();
			break;
		case rasctl_dev_scsi_hd:		// HDS/HDI
			pUnit = new SCSIHD();
			break;
		case rasctl_dev_scsi_hd_appl:   // HDA
			pUnit = new SCSIHD_APPLE();
			break;
		case rasctl_dev_scsi_hd_nec:    // HDN/NHD
			pUnit = new SCSIHD_NEC();
			break;
		case rasctl_dev_mo:
			pUnit = new SCSIMO();
			break;
		case rasctl_dev_cd:
			pUnit = new SCSICD();
			break;
		case rasctl_dev_br:
			pUnit = new SCSIBR();
			break;
		default:
			FPRT(fp,	"Error : Invalid device type\n");
			return FALSE;
	}

	// drive checks files
	switch(incoming_command->type){
		case rasctl_dev_mo:
		case rasctl_dev_scsi_hd_nec:
		case rasctl_dev_sasi_hd:
		case rasctl_dev_scsi_hd:
		case rasctl_dev_cd:
		case rasctl_dev_scsi_hd_appl:
			if((incoming_command->file != nullptr) && (strnlen(incoming_command->file,_MAX_PATH) > 0)){
				// Set the Path
				filepath.SetPath(incoming_command->file);

				// Open the file path
				if (!pUnit->Open(filepath)) {
					FPRT(fp, "Error : File open error [%s]\n", incoming_command->file);
					delete pUnit;
					return FALSE;
				}

			}
		break;
		case rasctl_dev_br:
		case rasctl_dev_invalid:
			// Do nothing
		break;
	}


	// Set the cache to write-through
	pUnit->SetCacheWB(FALSE);

	// Map the controller
	return Rascsi_Manager::AttachDevice(fp, pUnit, incoming_command->id, incoming_command->un);	
}

BOOL Command_Thread::DoDetach(FILE *fp, Rasctl_Command *incoming_command){
	return Rascsi_Manager::DetachDevice(fp, incoming_command->id, incoming_command->un);
}

BOOL Command_Thread::DoInsert(FILE *fp, Rasctl_Command *incoming_command){

	Filepath filepath;
	Disk *pUnit = Rascsi_Manager::GetDevice(fp, incoming_command->id, incoming_command->un);


	// Does the device exist?
	if (pUnit == nullptr) {
		FPRT(fp, "Error : No such device\n");
		return FALSE;
	}

	// Valid only for MO or CD
	if (pUnit->GetID() != MAKEID('S', 'C', 'M', 'O') &&
		pUnit->GetID() != MAKEID('S', 'C', 'C', 'D')) {
		FPRT(fp, "Error : Insert operation denied(Device isn't removable)\n");
		return FALSE;
	}

	// Set the file path
	filepath.SetPath(incoming_command->file);

	// Open the file
	if (!pUnit->Open(filepath)) {
		FPRT(fp, "Error : File open error [%s]\n", incoming_command->file);
		return FALSE;
	}
	
	return TRUE;
}
BOOL Command_Thread::DoEject(FILE *fp, Rasctl_Command *incoming_command){
	Disk *pUnit = Rascsi_Manager::GetDevice(fp, incoming_command->id, incoming_command->un);

	// Does the device exist?
	if (pUnit == nullptr) {
		FPRT(fp, "Error : No such device\n");
		return FALSE;
	}

	// Valid only for MO or CD
	if (pUnit->GetID() != MAKEID('S', 'C', 'M', 'O') &&
		pUnit->GetID() != MAKEID('S', 'C', 'C', 'D')) {
		FPRT(fp, "Error : Operation denied(Deveice isn't removable)\n");
		return FALSE;
	}
	pUnit->Eject(TRUE);
	return TRUE;
}

BOOL Command_Thread::DoProtect(FILE *fp, Rasctl_Command *incoming_command){

	Disk *pUnit = Rascsi_Manager::GetDevice(fp, incoming_command->id, incoming_command->un);

	// Does the device exist?
	if (pUnit == nullptr) {
		FPRT(fp, "Error : No such device\n");
		return FALSE;
	}

	// Valid only for MO
	if (pUnit->GetID() != MAKEID('S', 'C', 'M', 'O')) {
		FPRT(fp, "Error : Write Protect denied(Device isn't a MO drive)\n");
		return FALSE;
	}
	pUnit->WriteP(!pUnit->IsWriteP());
	return TRUE;
}











BOOL Command_Thread::HasSuffix(const char* string, const char* suffix) {
	int string_len = strlen(string);
	int suffix_len = strlen(suffix);
	return (string_len >= suffix_len)
		&& (xstrcasecmp(string + (string_len - suffix_len), suffix) == 0);
}

