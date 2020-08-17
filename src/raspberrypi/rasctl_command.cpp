#include "rasctl_command.h"
#include "rascsi_mgr.h"
#include <string.h>

// In the serialized string from rasctl
const char* Rasctl_Command::m_delimiter = "\x1E\n"; // Record separator charater
const char* Rasctl_Command::m_no_file = "----";
const uint16_t Rasctl_Command::PortNumber = 6868;

const char* Rasctl_Command::dev_type_lookup[] = {
    "SASI Hard Drive",       // rasctl_dev_sasi_hd =  0,
    "SCSI Hard Drive",       // rasctl_dev_scsi_hd =  1,
    "Magneto Optical Drive", // rasctl_dev_mo      =  2,
    "CD-ROM",                // rasctl_dev_cd      =  3,
    "Host Bridge",           // rasctl_dev_br      =  4,
    "Apple SCSI Hard Drive", // rasctl_dev_scsi_hd_appl =  5,
    "NEC SCSI Hard DRive",   // rasctl_dev_scsi_hd_nec  =  6,
};

void Rasctl_Command::Serialize(char *buff, int max_buff_size){
    // The filename string can't be empty. Otherwise, strtok will
    // ignore the field on the receiving end.
    if(strlen(this->file) < 1){
        strcpy(this->file,m_no_file);
    }
    snprintf(buff, max_buff_size, "%d%c%d%c%d%c%d%c%s%c\n",
                this->id, this->m_delimiter[0],
                this->un, this->m_delimiter[0],
                this->cmd, this->m_delimiter[0],
                this->type, this->m_delimiter[0],
                this->file, this->m_delimiter[0]);
}

Rasctl_Command* Rasctl_Command::DeSerialize(char* buff, int size){
    Rasctl_Command *return_command = new Rasctl_Command();
    char err_message[256];
    char *cur_token;
    char *command = buff;
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
                if(strncmp(cur_token,m_no_file,strlen(m_no_file)) != 0){
                    strncpy(return_command->file,cur_token,_MAX_PATH);
                } else {
                    strncpy(return_command->file,"",_MAX_PATH);
                }
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
    rasctl_dev_type expected_type = rasctl_dev_invalid;

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
        if(stat(file, &stat_buffer) != 0){
            FPRT(fp, "File %s is not accessible", file);
            return FALSE;
        }
        // Check that the file is 1 byte or larger
        if(stat_buffer.st_size < 1){
            FPRT(fp, "File must not be empty. %s is %lld bytes in size",file,stat_buffer.st_size);
            return FALSE;
        }
    }

    // Check if the filename extension matches the device type
    expected_type = DeviceTypeFromFilename(fp, file);
    if(expected_type != rasctl_dev_invalid){
        if((file != nullptr) && (type != expected_type)) {
            FPRT(fp, "Filename specified is for type %s. This isn't compatible with a %s device.",
                dev_type_lookup[expected_type], dev_type_lookup[type]);
            return FALSE;
        }
    }

    // Everything appears to be OK
    return TRUE;	
}


rasctl_dev_type Rasctl_Command::DeviceTypeFromFilename(FILE *fp, const char* filename){

    const char *extension = strrchr(filename,'.');
    rasctl_dev_type ret_type = rasctl_dev_invalid;

    if(extension == nullptr){
        fprintf(fp, "Missing file extension from %s",filename);
        return rasctl_dev_invalid;
    }

    if(strcasecmp(extension,".hdf") == 0){
        ret_type = rasctl_dev_sasi_hd;
    }else if((strcasecmp(extension,".hds") == 0) ||
            (strcasecmp(extension,".hdi") == 0) ||
            (strcasecmp(extension,".nhd") == 0)){
        ret_type = rasctl_dev_scsi_hd;
    }else if(strcasecmp(extension,".hdn") == 0){
        ret_type = rasctl_dev_scsi_hd_nec;
    }else if(strcasecmp(extension,".hdi") == 0){
        ret_type = rasctl_dev_scsi_hd_nec;
    }else if(strcasecmp(extension,".hda") == 0){
        ret_type = rasctl_dev_scsi_hd_appl;
    }else if(strcasecmp(extension,".mos") == 0){
        ret_type = rasctl_dev_mo;
    }else if(strcasecmp(extension,".iso") == 0){
        ret_type = rasctl_dev_cd;
    }

    return ret_type;
}

