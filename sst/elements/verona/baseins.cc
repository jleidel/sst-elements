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

#include "baseins.h"

using namespace SST::Verona;

Instruction::Instruction(uint32_t ins) {
	instruction = ins;
}

int Instruction::getOpCode() {
	return instruction & OP_CODE_MASK;	
}
