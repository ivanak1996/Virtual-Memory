#pragma once
#include"vm_declarations.h"

struct PMTEntry {

	bool valid;
	bool dirty;
	bool everUsed;
	bool hasReservedPartition;
	int frame;
	int clusterNo;
	AccessType accessType;

};