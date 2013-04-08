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

#ifndef _intalu_H
#define _intalu_H

namespace SST {
namespace Verona {

#define INT_OP_ADDI			0b0000010011
#define INT_OP_SLLI			0b0010010011
#define INT_OP_SLTI			0b0100010011
#define INT_OP_SLTIU		0b0110010011
#define INT_OP_XORI			0b1000010011
#define INT_OP_SRLI_OR_SRAI 0b1010010011
#define INT_OP_ORI			0b1100010011
#define INT_OP_ANDI			0b1110010011

#define INT_OP_ADD			0b00000000000110011
#define INT_OP_SUB			0b10000000000110011
#define INT_OP_SLL			0b00000000010110011
#define INT_OP_SLT			0b00000000100110011
#define INT_OP_SLTU			0b00000000110110011
#define INT_OP_XOR			0b00000001000110011
#define INT_OP_SRL			0b00000001010110011
#define INT_OP_SRA			0b10000001010110011
#define INT_OP_OR			0b00000001100110011
#define INT_OP_AND			0b00000001110110011
#define INT_OP_MUL			0b00000010000110011
#define INT_OP_MULH			0b00000010010110011
#define INT_OP_MULHSU		0b00000010100110011
#define INT_OP_MULHU		0b00000010110110011
#define INT_OP_DIV			0b00000011000110011
#define INT_OP_DIVU			0b00000011010110011
#define INT_OP_REM			0b00000011100110011
#define INT_OP_REMU			0b00000011110110011

}
}
#endif // _intalu_H
