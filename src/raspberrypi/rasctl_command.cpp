#include "rasctl_command.h"

//const char* Rasctl_Command::m_delimiter = "\x1E"; // Record separator charater
const char* Rasctl_Command::m_delimiter = " "; // Record separator charater

void Rasctl_Command::Serialize(BYTE *buff, int max_buff_size){
    snprintf((char*)buff, max_buff_size, "%d%s%d%s%d%s%d%s%s%s",
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
                for(int i=0; i<size; i++)
                {
                    snprintf(&err_message[i*2], sizeof(err_message), "%02X", buff[i]);
                }
                printf("Received too many tokens: %s", err_message);
                free(return_command);
                return nullptr;
            break;
        }
        cur_token_idx = (serial_token_order)((int)cur_token_idx + 1);
    }

    if(cur_token_idx != serial_token_last_token)
    {
        for(int i=0; i<size; i++)
        {
            snprintf(&err_message[i*2], sizeof(err_message), "%02X", buff[i]);
        }
        printf("Received too few tokens: %s", err_message);
        free(return_command);
    }

    return return_command;
}


BOOL Rasctl_Command::rasctl_dev_is_hd(rasctl_dev_type type)
{
    return ((type == rasctl_dev_scsi_hd) || (type == rasctl_dev_sasi_hd) || 
            (type == rasctl_dev_scsi_hd_appl) || (type == rasctl_dev_scsi_hd_nec));
}