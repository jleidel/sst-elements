// Copyright 2009-2014 Sandia Corporation. Under the terms
// of Contract DE-AC04-94AL85000 with Sandia Corporation, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2009-2014, Sandia Corporation
// All rights reserved.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.


#include <sst_config.h>

#include "emberhalo1d.h"

using namespace SST::Ember;

EmberHalo1DGenerator::EmberHalo1DGenerator(SST::Component* owner, Params& params) :
	EmberMessagePassingGenerator(owner, params),
	m_loopIndex(0)
{
	m_name = "Halo1D";

	iterations = (uint32_t) params.find_integer("arg.iterations", 10);
	nsCompute = (uint32_t) params.find_integer("arg.computenano", 1000);
	messageSize = (uint32_t) params.find_integer("arg.messagesize", 128);
}

bool EmberHalo1DGenerator::generate( std::queue<EmberEvent*>& evQ ) {

    if( 0 == m_loopIndex) {
        initOutput();
        GEN_DBG( 1, "rank=%d size=%d\n", rank(),size());
    }

		enQ_compute( evQ, nsCompute );

		if(0 == rank()) {
			enQ_recv( evQ, 1, messageSize, 0, GroupWorld);
			enQ_send( evQ, 1, messageSize, 0, GroupWorld);

		} else if( (size() - 1) == (signed )rank() ) {
			enQ_send( evQ, rank() - 1, messageSize, 0, GroupWorld);
			enQ_recv( evQ, rank() - 1, messageSize, 0, GroupWorld);

		} else {
			enQ_send( evQ, rank() - 1, messageSize, 0, GroupWorld);
			enQ_recv( evQ, rank() + 1, messageSize, 0, GroupWorld);
			enQ_send( evQ, rank() + 1, messageSize, 0, GroupWorld);
			enQ_recv( evQ, rank() - 1, messageSize, 0, GroupWorld);
		}

    if ( ++m_loopIndex == iterations ) {
        return true;
    } else {
        return false;
    }

}
