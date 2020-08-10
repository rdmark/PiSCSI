//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//	[ Send Control Command ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "rasctl_command.h"

//---------------------------------------------------------------------------
//
//	Send Command
//
//---------------------------------------------------------------------------
BOOL SendCommand(BYTE *buf)
{
	int fd;
	struct sockaddr_in server;
	FILE *fp;

	// Create a socket to send the command
	fd = socket(PF_INET, SOCK_STREAM, 0);
	memset(&server, 0, sizeof(server));
	server.sin_family = PF_INET;
	server.sin_port   = htons(6868);
	server.sin_addr.s_addr = htonl(INADDR_LOOPBACK); 

	// Connect
	if (connect(fd, (struct sockaddr *)&server,
		sizeof(struct sockaddr_in)) < 0) {
		fprintf(stderr, "Error : Can't connect to rascsi process\n");
		return FALSE;
	}

	// Send the command
	fp = fdopen(fd, "r+");
	setvbuf(fp, NULL, _IONBF, 0);
	fputs((char*)buf, fp);

	// Receive the message
	while (1) {
		if (fgets((char *)buf, BUFSIZ, fp) == NULL) {
			break;
		}
		printf("%s", buf);
	}

	// Close the socket when we're done
	fclose(fp);
	close(fd);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Main processing
//
//---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	int opt;
	int id;
	int un;
	char *file;
	int len;
	char *ext;
	BYTE buf[BUFSIZ];
	rasctl_command cmd = rasctl_cmd_invalid;
	rasctl_dev_type type = rasctl_dev_invalid;
	Rasctl_Command *rasctl_cmd;

	id = -1;
	un = 0;
	file = NULL;

	// Display help
	if (argc < 2) {
		fprintf(stderr, "SCSI Target Emulator RaSCSI Controller\n");
		fprintf(stderr,
			"Usage: %s -i ID [-u UNIT] [-c CMD] [-t TYPE] [-f FILE]\n",
			argv[0]);
		fprintf(stderr, " where  ID := {0|1|2|3|4|5|6|7}\n");
		fprintf(stderr, "        UNIT := {0|1} default setting is 0.\n");
		fprintf(stderr, "        CMD := {attach|detach|insert|eject|protect}\n");
		fprintf(stderr, "        TYPE := {hd|mo|cd|bridge}\n");
		fprintf(stderr, "        FILE := image file path\n");
		fprintf(stderr, " CMD is 'attach' or 'insert' and FILE parameter is required.\n");
		fprintf(stderr, "Usage: %s -l\n", argv[0]);
		fprintf(stderr, "       Print device list.\n\n");
		fprintf(stderr,"Build on %s at %s\n", __DATE__, __TIME__);
		exit(0);
	}

	// Parse the arguments
	opterr = 0;
	while ((opt = getopt(argc, argv, "i:u:c:t:f:l")) != -1) {
		switch (opt) {
			case 'i':
				id = optarg[0] - '0';
				break;

			case 'u':
				un = optarg[0] - '0';
				break;

			case 'c':
				switch (optarg[0]) {
					case 'a':				// ATTACH
					case 'A':
						cmd = rasctl_cmd_attach;
						break;
					case 'd':				// DETACH
					case 'D':
						cmd = rasctl_cmd_detach;
						break;
					case 'i':				// INSERT
					case 'I':
						cmd = rasctl_cmd_insert;
						break;
					case 'e':				// EJECT
					case 'E':
						cmd = rasctl_cmd_eject;
						break;
					case 'p':				// PROTECT
					case 'P':
						cmd = rasctl_cmd_protect;
						break;
					case 's':				// Shutdown the rasci service
					case 'S':
						cmd = rasctl_cmd_shutdown;
						break;
				}
				break;

			case 't':
				switch (optarg[0]) {
					case 's':				// HD(SASI)
					case 'S':
						type = rasctl_dev_sasi_hd;
						break;
					case 'h':				// HD(SCSI)
					case 'H':
						type = rasctl_dev_scsi_hd;
						break;
					case 'm':				// MO
					case 'M':
						type = rasctl_dev_mo;
						break;
					case 'c':				// CD
					case 'C':
						type = rasctl_dev_cd;
						break;
					case 'b':				// BRIDGE
					case 'B':
						type = rasctl_dev_br;
						break;
				}
				break;
			case 'f':
				file = optarg;
				break;
			case 'l':
				cmd = rasctl_cmd_list;
				break;
		}
	}

	if((cmd != rasctl_cmd_list) && (cmd != rasctl_cmd_shutdown)){

		// Check the ID number
		if (id < 0 || id > 7) {
			fprintf(stderr, "Error : Invalid ID\n");
			exit(EINVAL);
		}

		// Check the unit number
		if (un < 0 || un > 1) {
			fprintf(stderr, "Error : Invalid UNIT\n");
			exit(EINVAL);
		}

		// Command check
		if (cmd == rasctl_cmd_invalid) {
			cmd = rasctl_cmd_attach;	// Default command is ATTATCH
		}

		// Type Check
		if (cmd == rasctl_cmd_attach && type == rasctl_dev_invalid) {

			// Try to determine the file type from the extension
			len = file ? strlen(file) : 0;
			if (len > 4 && file[len - 4] == '.') {
				ext = &file[len - 3];
				if (xstrcasecmp(ext, "hdf") == 0){
					type = rasctl_dev_sasi_hd;
				}else if(xstrcasecmp(ext, "hds") == 0 ||
					xstrcasecmp(ext, "hdi") == 0 ||
					xstrcasecmp(ext, "nhd") == 0){
					type = rasctl_dev_scsi_hd;
				}else if(xstrcasecmp(ext, "hdn") == 0) {
					// NEC HD(SASI/SCSI)
					type = rasctl_dev_scsi_hd_appl;
				}else if(xstrcasecmp(ext, "hda") == 0) {
					// Apple HD(SASI/SCSI)
					type = rasctl_dev_scsi_hd_appl;
				}else if (xstrcasecmp(ext, "mos") == 0) {
					// MO
					type = rasctl_dev_mo;
				}else if (xstrcasecmp(ext, "iso") == 0) {
					// CD
					type = rasctl_dev_cd;
				}
			}

			if (type == rasctl_dev_invalid) {
				fprintf(stderr, "Error : Invalid type\n");
				exit(EINVAL);
			}
		}

		// File check (command is ATTACH and type is HD)
		if ((cmd == rasctl_cmd_attach) && (Rasctl_Command::rasctl_dev_is_hd(type))){
			if (!file) {
				fprintf(stderr, "Error : Invalid file path\n");
				exit(EINVAL);
			}
		}

		// File check (command is INSERT)
		if (cmd == rasctl_cmd_insert) {
			if (!file) {
				fprintf(stderr, "Error : Invalid file path\n");
				exit(EINVAL);
			}
		}

		// If we don't know what the type is, default to SCSI HD
		if (type == rasctl_dev_invalid) {
			type = rasctl_dev_scsi_hd;
		}
	}

	rasctl_cmd = new Rasctl_Command();
	rasctl_cmd->type = type;
	rasctl_cmd->un = un;
	rasctl_cmd->id = id;
	rasctl_cmd->cmd = cmd;
	if(file){
		strncpy(rasctl_cmd->file,file,_MAX_PATH);
	}

	// Generate the command and send it
	rasctl_cmd->Serialize(buf,BUFSIZ);

	if (!SendCommand(buf)) {
		exit(ENOTCONN);
	}

	// All done!
	exit(0);
}
