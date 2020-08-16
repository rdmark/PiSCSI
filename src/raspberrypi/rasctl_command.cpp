#include "rasctl_command.h"
#include "rascsi_mgr.h"

const char* Rasctl_Command::m_delimiter = "\x1E"; // Record separator charater
//const char* Rasctl_Command::m_delimiter = " "; // Record separator charater

void Rasctl_Command::Serialize(BYTE *buff, int max_buff_size){
    snprintf((char*)buff, max_buff_size, "%d%s%d%s%d%s%d%s%s%s\n",
                this->id, this->m_delimiter,
                this->un, this->m_delimiter,
                this->cmd, this->m_delimiter,
                this->type, this->m_delimiter,
                this->file, this->m_delimiter);
}

Rasctl_Command* Rasctl_Command::DeSerialize(BYTE* buff, int size){
    Rasctl_Command *return_command = new Rasctl_Command();
    char err_message[256];
    char *cur_token;
    char *command = (char*)buff;
    serial_token_order cur_token_idx = serial_token_first_token;

    cur_token = strtok(command, m_delimiter);
    // Loop through all of the tokens in the message
    while (cur_token != NULL){
        switch(cur_token_idx){
            case serial_token_cmd:
                return_command->cmd = (rasctl_command)atoi(cur_token);
            break;
            case serial_token_type:
                return_command->type = (rasctl_dev_type)atoi(cur_token);
            break;
            case serial_token_file_name:
                strncpy(return_command->file,cur_token,_MAX_PATH);
            break;
            case serial_token_id:
                return_command->id = atoi(cur_token);
            break;
            case serial_token_un:
                return_command->un = atoi(cur_token);
            break;
            default:
                for(size_t i=0; i<strlen((char*)buff); i++)
                {
                    snprintf(&err_message[i*2], sizeof(err_message), "%02X", buff[i]);
                }
                printf("Received too many tokens: %s", err_message);
                free(return_command);
                return nullptr;
            break;
        }
        cur_token_idx = (serial_token_order)((int)cur_token_idx + 1);
        cur_token = strtok(NULL, m_delimiter);
    }

    if(cur_token_idx != serial_token_last_token)
    {
        for(size_t i=0; i<strlen((char*)buff); i++)
        {
            snprintf(&err_message[i*2], sizeof(err_message), "%02X", buff[i]);
        }
        printf("Received too few tokens: %s", err_message);
        free(return_command);
        return nullptr;
    }

    return return_command;
}


BOOL Rasctl_Command::rasctl_dev_is_hd(rasctl_dev_type type)
{
    return ((type == rasctl_dev_scsi_hd) || (type == rasctl_dev_sasi_hd) || 
            (type == rasctl_dev_scsi_hd_appl) || (type == rasctl_dev_scsi_hd_nec));
}



BOOL Rasctl_Command::IsValid(FILE *fp){
    struct stat stat_buffer;

    // If the command is "invalid" we can just return FALSE
    if(cmd == rasctl_cmd_invalid){
        return FALSE;
    }

    // If the command is "list" or "shutdown", we don't need to validate the rest of the arguments
    if((cmd == rasctl_cmd_list) || (cmd == rasctl_cmd_shutdown)) {
        return TRUE;
    }

    // Check that the ID is in range
    if((id < 0) || (id >= Rascsi_Manager::CtrlMax)){
        FPRT(fp, "Invalid ID. Must be between 0 and %d",Rascsi_Manager::CtrlMax-1);
        return FALSE;
    }

    // Check that unit number is in range
    if((un < 0) || (un >= Rascsi_Manager::UnitNum)){
        FPRT(fp, "Invalid unit number. Must be between 0 and %d",Rascsi_Manager::UnitNum-1);
        return FALSE;
    }

    // If the command is attach, make sure we're adding a valid type
    if(cmd == rasctl_cmd_attach){
        if(type == rasctl_dev_invalid){
            FPRT(fp, "Unknown device type being attached");
            return FALSE;
        }
    }

    // If we're trying to insert a disk, we need to make sure a file is specified.
    // Also, if we're trying to attach a hard drive, the image must be specified
    if((cmd == rasctl_cmd_insert) || ((cmd == rasctl_cmd_attach) && (rasctl_dev_is_hd(type)))){
        if(file == nullptr){
            FPRT(fp, "File name must be specified for this operation");
            return FALSE;
        }
        // Check that the file exists if specified
        if(stat(file, &stat_buffer) == 0){
            FPRT(fp, "File %s is not accessible", file);
            return FALSE;
        }
        // Check that the file is 1 byte or larger
        if(stat_buffer.st_size < 1){
            FPRT(fp, "File must not be empty. %s is %ld bytes in size",file,stat_buffer.st_size);
            return FALSE;
        }
    }

// 		char* path = optarg;
// 		int type = -1;
// 		if (HasSuffix(path, ".hdf")
// 			|| HasSuffix(path, ".hds")
// 			|| HasSuffix(path, ".hdn")
// 			|| HasSuffix(path, ".hdi")
// 			|| HasSuffix(path, ".hda")
// 			|| HasSuffix(path, ".nhd")) {
// 			type = 0;
// 		} else if (HasSuffix(path, ".mos")) {
// 			type = 2;
// 		} else if (HasSuffix(path, ".iso")) {
// 			type = 3;
// 		} else if (xstrcasecmp(path, "bridge") == 0) {
// 			type = 4;
// 		} else {
// 			// Cannot determine the file type
// 			fprintf(stderr,
// 					"%s: unknown file extension\n", path);
// 			return false;
// 		}

// 		int un = 0;
// 		if (is_sasi) {
// 			un = id % Rascsi_Manager::UnitNum;
// 			id /= Rascsi_Manager::UnitNum;
// 		}

// 		// Execute the command
// 		if (!ExecuteCommand(stderr, id, un, 0, type, path)) {
// 			return false;
// 		}
// 		id = -1;
// 	}

// 	// Display the device list
// 	Rascsi_Manager::GetInstance()->ListDevice(stdout);
// 	return true;
// }






		// if (type == 0) {
		// 	// Passed the check
		// 	if (!file) {
		// 		return FALSE;
		// 	}

		// 	// Check that command is at least 5 characters long
		// 	len = strlen(file);
		// 	if (len < 5) {
		// 		return FALSE;
		// 	}

		// 	// Check the extension
		// 	if (file[len - 4] != '.') {
		// 		return FALSE;
		// 	}

		// 	// If the extension is not SASI type, replace with SCSI
		// 	ext = &file[len - 3];
		// 	if (xstrcasecmp(ext, "hdf") != 0) {
		// 		type = 1;
		// 	}

return FALSE;
	
}
