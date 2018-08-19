#include"KernelProcess.h"
#include"KernelSystem.h"
#include<cstring>

KernelProcess::KernelProcess(ProcessId pid) {
	this->pid = pid;
	this->descriptorHead = nullptr;

}

KernelProcess::~KernelProcess() {
	SegmentDescriptor *current;

	while (descriptorHead) {
		current = descriptorHead;
		descriptorHead = descriptorHead->next;
		delete current;
	}
}

ProcessId KernelProcess::getProcessId() const {
	return pid;
}

Status KernelProcess::createSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags) {
	SegmentDescriptor *current = descriptorHead;

	if ((startAddress % PAGE_SIZE) != 0 || (startAddress + segmentSize * PAGE_SIZE >= 2<<24)) return TRAP;

	while (current) {
		if (current->startingAddress <= startAddress && current->startingAddress + current->pageNum*PAGE_SIZE > startAddress) return TRAP;
		if (startAddress <= current->startingAddress && startAddress + segmentSize * PAGE_SIZE > current->startingAddress) return TRAP;
		
		current = current->next;
	}

	SegmentDescriptor *newOne = new SegmentDescriptor();
	newOne->startingAddress = startAddress;
	newOne->pageNum = segmentSize;
	newOne->access = flags;
	newOne->content = nullptr;
	
	current = descriptorHead;
	SegmentDescriptor *prev = nullptr;

	while ((current != nullptr) && (startAddress > current->startingAddress)) {
		prev = current;
		current = current->next;
	}

	if (prev == nullptr) {
		newOne->next = descriptorHead;
		descriptorHead = newOne;
	}
	else {
		newOne->next = current;
		prev->next = newOne;
	}

	for (unsigned i = startAddress >> 10; i < (startAddress >> 10) + segmentSize; i++)
		pmtp[i].accessType = flags;

	return OK;

}

Status KernelProcess::loadSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags, void* content) {
	SegmentDescriptor *current = descriptorHead;

	if ((startAddress % PAGE_SIZE) != 0 || (startAddress + segmentSize * PAGE_SIZE >= 2 << 24)) return TRAP;

	while (current) {
		if (current->startingAddress <= startAddress && current->startingAddress + current->pageNum*PAGE_SIZE > startAddress) return TRAP;
		if (startAddress <= current->startingAddress && startAddress + segmentSize * PAGE_SIZE > current->startingAddress) return TRAP;

		current = current->next;
	}

	SegmentDescriptor *newOne = new SegmentDescriptor();
	newOne->startingAddress = startAddress;
	newOne->pageNum = segmentSize;
	newOne->access = flags;
	newOne->content = (char*)content;

	current = descriptorHead;
	SegmentDescriptor *prev = nullptr;

	while ((current != nullptr) && (startAddress > current->startingAddress)) {
		prev = current;
		current = current->next;
	}

	if (prev == nullptr) {
		newOne->next = descriptorHead;
		descriptorHead = newOne;
	}
	else {
		newOne->next = current;
		prev->next = newOne;
	}

	for (unsigned i = startAddress >> 10; i < (startAddress >> 10) + segmentSize; i++)
		pmtp[i].accessType = flags;

	return OK;
}


Status KernelProcess::deleteSegment(VirtualAddress startAddress) {

	SegmentDescriptor *current = descriptorHead;

	if ((startAddress % PAGE_SIZE) != 0 || (startAddress >= 2 << 24)) return TRAP;

	SegmentDescriptor *prev = nullptr;
	while (current) {
		
		if (current->startingAddress == startAddress) {
			if (current == descriptorHead) {
				descriptorHead = descriptorHead->next;
			}
			else {
				prev->next = current->next;
				current->next = nullptr;
			}

			for (unsigned i = current->startingAddress >> 10; i < (current->startingAddress >> 10) + current->pageNum; i++) {
				if (pmtp[i].valid) KernelSystem::frameEntries[pmtp[i].frame] = nullptr;
				if (pmtp[i].hasReservedPartition) KernelSystem::freePartitionClusters.push(pmtp[i].clusterNo);
				memset(pmtp + i, 0, 16);
			}

			//memset((void*)((unsigned)pmtp + current->startingAddress), 0, current->pageNum*PAGE_SIZE);
			delete current;
			return OK;
		}

		prev = current;
		current = current->next;
	}

	return TRAP;
}

PhysicalAddress KernelProcess::getPhysicalAddress(VirtualAddress address) {

	return (PhysicalAddress)((VirtualAddress)KernelSystem::processVMSpace + pmtp[address >> 10].frame*PAGE_SIZE + (address & 1023));

}

bool checkAccessRight(AccessType requestedType, AccessType allowedType) {
	return true;
	if (requestedType == allowedType) return true;
	if ((requestedType == AccessType::READ || requestedType == AccessType::WRITE) && allowedType == AccessType::READ_WRITE) return true;

	return false;
}

Status KernelProcess::pageFault(VirtualAddress address) {

	unsigned int shiftedAddress = address >> 10;
	PMTEntry& entry = pmtp[shiftedAddress];

	if (!checkAccessRight(this->lastType, entry.accessType))
		return TRAP;

	PMTEntry* victimEntry = KernelSystem::frameEntries[KernelSystem::victim];

	if (victimEntry) {
		if (victimEntry->valid) {
			victimEntry->valid = false;
			if (victimEntry->dirty||!victimEntry->hasReservedPartition) {
				if (!victimEntry->hasReservedPartition) {
					if (KernelSystem::freePartitionClusters.empty())
						return TRAP;
					victimEntry->clusterNo = KernelSystem::freePartitionClusters.front();
					victimEntry->hasReservedPartition = true;
					KernelSystem::freePartitionClusters.pop();
				}
				victimEntry->dirty = false;
				if (victimEntry->hasReservedPartition) {
					if (KernelSystem::partition->writeCluster(victimEntry->clusterNo,
						(char*)(victimEntry->frame*PAGE_SIZE + (unsigned)KernelSystem::processVMSpace)) == 0)
						return TRAP;
				}
			}
		}

	}

	if (entry.everUsed) {
		if (KernelSystem::partition->readCluster(entry.clusterNo,
			(char*)(KernelSystem::victim*PAGE_SIZE + (unsigned)KernelSystem::processVMSpace)) == 0)
			return TRAP;

	}
	else {

		SegmentDescriptor *current = descriptorHead;

		while (current) {
			if (current->startingAddress <= address && current->startingAddress + current->pageNum*PAGE_SIZE > address) break;
			current = current->next;
		}

		if (current == nullptr)
			return TRAP;

		if (current->content != nullptr)
			std::memcpy((char*)(KernelSystem::victim*PAGE_SIZE + (unsigned)KernelSystem::processVMSpace), current->content + ((address & ~1023) - current->startingAddress), PAGE_SIZE);
		entry.everUsed = true;

	}

	entry.valid = true;
	entry.frame = KernelSystem::victim;
	entry.dirty = false;
	KernelSystem::frameEntries[KernelSystem::victim] = &pmtp[shiftedAddress];
	KernelSystem::victim = (KernelSystem::victim + 1) % KernelSystem::processVMSpaceSize;

	return OK;
}

