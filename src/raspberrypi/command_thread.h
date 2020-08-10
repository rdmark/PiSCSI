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
#pragma once

#include "xm6.h"
#include "os.h"
#include "rasctl_command.h"

class Command_Thread{

    public:
        static BOOL Init();
        static void Start();
        static void Stop();
        static BOOL IsRunning();
        static void Close();
        static void KillHandler();
        static BOOL ParseArgument(int argc, char* argv[]);
        static BOOL ParseConfig(int argc, char* argv[]);
        static BOOL ExecuteCommand(FILE *fp, Rasctl_Command *new_command);
        static void *MonThread(void *param);
        static BOOL Parse_Rasctl_Message(char *buf, Rasctl_Command *cmd);

    private:
        static int m_monsocket;						// Monitor Socket
        static pthread_t m_monthread;				// Monitor Thread
        static BOOL m_running;

        static BOOL HasSuffix(const char* string, const char* suffix);
        static BOOL DoShutdown(FILE *fp, Rasctl_Command *incoming_command);
        static BOOL DoList(FILE *fp, Rasctl_Command *incoming_command);
        static BOOL DoAttach(FILE *fp, Rasctl_Command *incoming_command);
        static BOOL DoDetach(FILE *fp, Rasctl_Command *incoming_command);
        static BOOL DoInsert(FILE *fp, Rasctl_Command *incoming_command);
        static BOOL DoEject(FILE *fp, Rasctl_Command *incoming_command);
        static BOOL DoProtect(FILE *fp, Rasctl_Command *incoming_command);

};

