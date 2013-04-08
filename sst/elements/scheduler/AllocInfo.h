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

/*
 * Classes representing information about an allocation
 */

#ifndef __ALLOCINFO_H__
#define __ALLOCINFO_H__

#include "Job.h"

namespace SST {
namespace Scheduler {

class Job;
class Machine;

class AllocInfo {
 public:
  Job* job;
  int* nodeIndices;

  AllocInfo(Job* job);

  virtual ~AllocInfo();

  virtual string getProcList();
};


}
}
#endif
