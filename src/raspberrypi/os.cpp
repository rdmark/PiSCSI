//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//  Copyright (C) 2020 akuker
//
//	[ OS specific features / glue logic ]
//
//---------------------------------------------------------------------------
#include "os.h"

//---------------------------------------------------------------------------
//
//	Pin the thread to a specific CPU
//
//---------------------------------------------------------------------------
void FixCpu(int cpu)
{
#if !defined BAREMETAL && defined(__linux__)
	cpu_set_t cpuset;
	int cpus;

	// Get the number of CPUs
	CPU_ZERO(&cpuset);
	sched_getaffinity(0, sizeof(cpu_set_t), &cpuset);
	cpus = CPU_COUNT(&cpuset);

	// Set the thread affinity
	if (cpu < cpus) {
		CPU_ZERO(&cpuset);
		CPU_SET(cpu, &cpuset);
		sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
	}
#else
	// RaSCSI itself only functions on a Raspberry Pi. However, the
	// unit tests should run on other hosts. For example, the github
	// action runners are only Ubuntu x86_64. Some developers may also
	// want to be able to run the unit tests on MacOS.
    return;
#endif
}
