#pragma once

#include"vm_declarations.h"
#include"KernelProcess.h"
#include"part.h"
#include <queue>
#include<vector>

class Process;
class Partition;

class KernelSystem {
public:
	KernelSystem(PhysicalAddress processVMSpace, PageNum processVMSpaceSize, PhysicalAddress pmtSpace, PageNum pmtSpaceSize, Partition* partition);
	~KernelSystem();
	Process* createProcess();
	bool checkAccessRight(AccessType requestedType, AccessType allowedType) const;

	Time periodicJob();
	// Hardware job
	Status access(ProcessId pid, VirtualAddress address, AccessType type);

private:
	friend class KernelProcess;

	static PhysicalAddress processVMSpace;
	static unsigned victim;
	static PageNum processVMSpaceSize;
	PhysicalAddress pmtSpace;
	PageNum pmtSpaceSize;
	static Partition* partition;

	static PMTEntry **frameEntries;

	std::vector<Process*> processes;
	static std::queue<ClusterNo> freePartitionClusters;
	std::queue<unsigned int> freePmtQueue;

};