// -*- mode: c++ -*-

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


#ifndef COMPONENTS_MERLIN_TOPOLOGY_DRAGONFLY_H
#define COMPONENTS_MERLIN_TOPOLOGY_DRAGONFLY_H

#include <sst/core/event.h>
#include <sst/core/link.h>
#include <sst/core/params.h>

#include "sst/elements/merlin/router.h"

namespace SST {
namespace Merlin {

class topo_dragonfly_event;

class topo_dragonfly: public Topology {

    struct dgnflyParams {
        uint32_t p;  /* # of hosts / router */
        uint32_t a;  /* # of routers / group */
        uint32_t k;  /* Router Radix */
        uint32_t h;  /* # of ports / router to connect to other groups */
        uint32_t g;  /* # of Groups */
    };

    enum RouteAlgo {
        MINIMAL,
        VALIANT
    };


    struct dgnflyParams params;
    RouteAlgo algorithm;
    uint32_t group_id;
    uint32_t router_id;

public:
    struct dgnflyAddr {
        uint32_t group;
        uint32_t mid_group;
        uint32_t router;
        uint32_t host;
    };

    topo_dragonfly(Params& p);
    ~topo_dragonfly();

    virtual void route(int port, int vc, internal_router_event* ev);
    virtual internal_router_event* process_input(RtrEvent* ev);
    virtual PortState getPortState(int port) const;

private:
    void idToLocation(int id, dgnflyAddr *location) const;
    uint32_t router_to_group(uint32_t group) const;
    uint32_t port_for_router(uint32_t router) const;
    uint32_t port_for_group(uint32_t group) const;
};




class topo_dragonfly_event : public internal_router_event {

public:

    topo_dragonfly::dgnflyAddr dest;

    topo_dragonfly_event(const topo_dragonfly::dgnflyAddr &dest) : dest(dest) {}
    ~topo_dragonfly_event() { }
};

}
}

#endif // COMPONENTS_MERLIN_TOPOLOGY_DRAGONFLY_H
