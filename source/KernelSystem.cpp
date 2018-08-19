#include "KernelSystem.h"
#include"Process.h"
#include <cstring>

PhysicalAddress KernelSystem::processVMSpace = nullptr;
PageNum KernelSystem::processVMSpaceSize = 0;
unsigned KernelSystem::victim = 0;
PMTEntry ** KernelSystem::frameEntries = nullptr;
Partition* KernelSystem::partition = nullptr;
std::queue<ClusterNo> KernelSystem::freePartitionClusters;

KernelSystem::KernelSystem(PhysicalAddress processVMSpace, PageNum processVMSpaceSize, PhysicalAddress pmtSpace, 
	PageNum pmtSpaceSize, Partition* partition) {

	this->processVMSpace = processVMSpace;
	this->processVMSpaceSize = processVMSpaceSize;
	this->pmtSpace = pmtSpace;
	this->pmtSpaceSize = pmtSpaceSize;
	this->partition = partition;
	
	std::memset(pmtSpace, 0, pmtSpaceSize*PAGE_SIZE);

	for (int i = 0; i < partition->getNumOfClusters(); i++) {
		freePartitionClusters.push(i);
	}

	for (int i = 0; i < pmtSpaceSize; i += 256) {
		freePmtQueue.push(i);
	}

	frameEntries = new PMTEntry*[processVMSpaceSize];
	for (int i = 0; i < processVMSpaceSize; i++) {
		frameEntries[i] = nullptr;
	}

}

Process* KernelSystem::createProcess() {

	if (freePmtQueue.empty() || freePmtQueue.front() + 256 > pmtSpaceSize) return nullptr;
	ProcessId pid = 0;

	for (pid = 0; pid < processes.size(); pid++) {
		if (processes[pid] == nullptr) break;
	}

	Process *newProcess = new Process(pid);
	if (pid == processes.size())
		processes.push_back(newProcess);
	else
		processes[pid] = newProcess;

	newProcess->pProcess->pmtp = (PMTEntry*)((char*)pmtSpace + freePmtQueue.front() * PAGE_SIZE);
	freePmtQueue.pop();

	return newProcess;

}

KernelSystem::~KernelSystem() {
	for (int i = 0; i < processes.size(); i++) {
		if (processes.at(i) != nullptr) {
			delete processes.at(i);
		}
	}
	delete frameEntries;
}

Status KernelSystem::access(ProcessId pid, VirtualAddress address, AccessType type) {
	if (pid >= processes.size() || processes.at(pid) == nullptr || address >= 2<<24) return TRAP;

	unsigned int shiftedAddress = address >> 10;
	PMTEntry& entry = (processes.at(pid))->pProcess->pmtp[shiftedAddress];
	processes[pid]->pProcess->lastType = type;
	if (checkAccessRight(type, entry.accessType)) {
		if (entry.everUsed && entry.valid) {
			if (type == WRITE)
				entry.dirty = true;
			return OK;
		}
		else {
			return PAGE_FAULT;
		}
	}
	else {
		return TRAP;
	}
}

bool KernelSystem::checkAccessRight(AccessType requestedType, AccessType allowedType) const {
	return true;
	if (requestedType == allowedType) return true;
	if ((requestedType == AccessType::READ || requestedType == AccessType::WRITE) && allowedType == AccessType::READ_WRITE) return true;

	return false;
}


Time KernelSystem::periodicJob() {
	return 0;
}