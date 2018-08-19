#pragma once
#include"vm_declarations.h"
#include"PMTEntry.h"

class KernelProcess {
public:
	KernelProcess(ProcessId);
	~KernelProcess();
	ProcessId getProcessId() const;
	Status createSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags);
	Status loadSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags, void* content);
	Status deleteSegment(VirtualAddress startAddress);
	Status pageFault(VirtualAddress address);
	PhysicalAddress getPhysicalAddress(VirtualAddress address);

private:

	struct SegmentDescriptor {
		VirtualAddress startingAddress;
		PageNum pageNum;
		AccessType access;
		char *content;
		SegmentDescriptor *next;
	};

	AccessType lastType;
	SegmentDescriptor *descriptorHead;
	ProcessId pid;
	PMTEntry *pmtp;
	friend class KernelSystem;

};