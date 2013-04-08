// Copyright 2009-2013 Sandia Corporation. Under the terms
// of Contract DE-AC04-94AL85000 with Sandia Corporation, the U.S.
// Government retains certain rights in this software.
// 
// Copyright (c) 2009-2013, Sandia Corporation
// All rights reserved.
// 
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.

/**
 * Allocator that assigns the first available processors (according to
 * order specified when allocator is created).
 */
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <time.h>
#include <math.h>

#include "sst/core/serialization/element.h"

#include "SortedFreeListAllocator.h"
#include "LinearAllocator.h"
#include "Machine.h"
#include "MachineMesh.h"
#include "AllocInfo.h"
#include "Job.h"
#include "misc.h"

#define DEBUG false

using namespace SST::Scheduler;


SortedFreeListAllocator::SortedFreeListAllocator(vector<string>* params, Machine* mach) : LinearAllocator(params, mach){
  if(DEBUG)
    printf("Constructing SortedFreeListAllocator\n");
  if(dynamic_cast<MachineMesh*>(mach) == NULL)
    error("Linear allocators require a mesh");
}

string SortedFreeListAllocator::getSetupInfo(bool comment){
  string com;
  if(comment) com="# ";
  else com="";
  return com+"Linear Allocator (Sorted Free List)";
}

AllocInfo* SortedFreeListAllocator::allocate(Job* job) {
  //allocates j if possible
  //returns information on the allocation or NULL if it wasn't possible
  //(doesn't make allocation; merely returns info on possible allocation)
  if(DEBUG)
    printf("Allocating %s \n", job->toString().c_str());

  if(!canAllocate(job))
    return NULL;

  vector<MeshLocation*>* freeprocs = ((MachineMesh*)machine)->freeProcessors();
  stable_sort(freeprocs->begin(), freeprocs->end(), *ordering);

  int num = job->getProcsNeeded();  //number of processors for job

  MeshAllocInfo* retVal = new MeshAllocInfo(job);
  for(int i=0; i<(int)freeprocs->size(); i++)
  {
    if(i < num)
    {
      retVal->processors->at(i) = freeprocs->at(i);
      retVal->nodeIndices[i] = freeprocs->at(i)->toInt((MachineMesh*)machine);
    }
    else
      delete freeprocs->at(i);
  }
  delete freeprocs;
  return retVal;
}
