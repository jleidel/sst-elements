// Copyright 2009-2010 Sandia Corporation. Under the terms
// of Contract DE-AC04-94AL85000 with Sandia Corporation, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2009-2010, Sandia Corporation
// All rights reserved.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.


#ifndef _GHOST_PATTERN_H
#define _GHOST_PATTERN_H

#include <sst/core/event.h>
#include <sst/core/component.h>
#include <sst/core/link.h>
#include "pattern_common.h"

using namespace SST;

#define DBG_GHOST_PATTERN 1
#if DBG_GHOST_PATTERN
#define _GHOST_PATTERN_DBG(lvl, fmt, args...)\
    if (ghost_pattern_debug >= lvl)   { \
	printf("%d:Ghost_pattern::%s():%4d: " fmt, _debug_rank, __FUNCTION__, __LINE__, ## args); \
    }
#else
#define _GHOST_PATTERN_DBG(lvl, fmt, args...)
#endif

typedef enum {INIT,			// First state in state machine
              COMPUTE,			// We are currently computing
	      WAIT,			// Waiting for one or more messages
	      DONE,			// Work is all done
	      CHCKPT,			// Writing a checkpoint
	      SAVE_ENVELOPE,		// Save receive msg envelope
	      LOG_MSG1,			// Log the first message to stable (local) storage
	      LOG_MSG2,			// Log the second message to stable (local) storage
	      LOG_MSG3,			// Log the third message to stable (local) storage
	      LOG_MSG4			// Log the fourth message to stable (local) storage
} state_t;



class Ghost_pattern : public Component {
    public:
        Ghost_pattern(ComponentId_t id, Params_t& params) :
            Component(id),
            params(params)
        {

            Params_t::iterator it= params.begin();

	    // Defaults for paramters
	    ghost_pattern_debug= 0;
	    my_rank= -1;
	    net_latency= 0;
	    net_bandwidth= 0;
	    node_latency= 0;
	    node_bandwidth= 0;
	    compute_time= 0;
	    application_end_time= 0;
	    exchange_msg_len= 128;
	    cores= -1;
	    chckpt_method= CHCKPT_NONE;
	    chckpt_interval= 0;
	    envelope_size= 0;
	    chckpt_size= 0;
	    msg_write_time= 0;

	    // Counters, computed values, and internal state
	    execution_time= 0;
	    msg_wait_time= 0;
	    chckpt_time= 0;
	    timestep_cnt= 0;
	    state= INIT;
	    rcv_cnt= 0;
	    save_ENVELOPE_cnt= 0;
	    application_time_so_far= 0;
	    chckpt_steps= 1;
	    application_done= FALSE;
	    num_chckpts= 0;
	    total_rcvs= 0;



	    registerExit();

            while (it != params.end())   {
                _GHOST_PATTERN_DBG(2, "[%3d] key \"%s\", value \"%s\"\n", my_rank,
		    it->first.c_str(), it->second.c_str());

		if (!it->first.compare("debug"))   {
		    sscanf(it->second.c_str(), "%d", &ghost_pattern_debug);
		}

		if (!it->first.compare("rank"))   {
		    sscanf(it->second.c_str(), "%d", &my_rank);
		}

		if (!it->first.compare("x_dim"))   {
		    sscanf(it->second.c_str(), "%d", &x_dim);
		}

		if (!it->first.compare("y_dim"))   {
		    sscanf(it->second.c_str(), "%d", &y_dim);
		}

		if (!it->first.compare("NoC_x_dim"))   {
		    sscanf(it->second.c_str(), "%d", &NoC_x_dim);
		}

		if (!it->first.compare("NoC_y_dim"))   {
		    sscanf(it->second.c_str(), "%d", &NoC_y_dim);
		}

		if (!it->first.compare("cores"))   {
		    sscanf(it->second.c_str(), "%d", &cores);
		}

		if (!it->first.compare("net_latency"))   {
		    sscanf(it->second.c_str(), "%lu", &net_latency);
		}

		if (!it->first.compare("net_bandwidth"))   {
		    sscanf(it->second.c_str(), "%lu", &net_bandwidth);
		}

		if (!it->first.compare("node_latency"))   {
		    sscanf(it->second.c_str(), "%lu", &node_latency);
		}

		if (!it->first.compare("node_bandwidth"))   {
		    sscanf(it->second.c_str(), "%lu", &node_bandwidth);
		}

		if (!it->first.compare("compute_time"))   {
		    sscanf(it->second.c_str(), "%lu", &compute_time);
		}

		if (!it->first.compare("application_end_time"))   {
		    sscanf(it->second.c_str(), "%lu", &application_end_time);
		}

		if (!it->first.compare("exchange_msg_len"))   {
		    sscanf(it->second.c_str(), "%d", &exchange_msg_len);
		}

		if (!it->first.compare("chckpt_method"))   {
		    if (!it->second.compare("none"))   {
			chckpt_method= CHCKPT_NONE;
		    } else if (!it->second.compare("coordinated"))   {
			chckpt_method= CHCKPT_COORD;
		    } else if (!it->second.compare("uncoordinated"))   {
			chckpt_method= CHCKPT_UNCOORD;
		    } else if (!it->second.compare("distributed"))   {
			chckpt_method= CHCKPT_RAID;
		    }
		}

		if (!it->first.compare("chckpt_interval"))   {
		    sscanf(it->second.c_str(), "%lu", &chckpt_interval);
		}

		if (!it->first.compare("envelope_size"))   {
		    sscanf(it->second.c_str(), "%d", &envelope_size);
		}

		if (!it->first.compare("msg_write_time"))   {
		    sscanf(it->second.c_str(), "%lu", &msg_write_time);
		}

		if (!it->first.compare("chckpt_size"))   {
		    sscanf(it->second.c_str(), "%d", &chckpt_size);
		}

                ++it;
            }


	    // Create a time converter
	    tc= registerTimeBase("1ns", true);

            // Create a handler for events from the Network
	    net= configureLink("NETWORK", new Event::Handler<Ghost_pattern>
		    (this, &Ghost_pattern::handle_net_events));
	    if (net == NULL)   {
		_GHOST_PATTERN_DBG(1, "There is no network...\n");
	    } else   {
		net->setDefaultTimeBase(tc);
		_GHOST_PATTERN_DBG(2, "[%3d] Added a link and a handler for the Network\n", my_rank);
	    }

            // Create a handler for events from the network on chip (NoC)
	    NoC= configureLink("NoC", new Event::Handler<Ghost_pattern>
		    (this, &Ghost_pattern::handle_NoC_events));
	    if (NoC == NULL)   {
		_GHOST_PATTERN_DBG(1, "There is no NoC...\n");
	    } else   {
		NoC->setDefaultTimeBase(tc);
		_GHOST_PATTERN_DBG(2, "[%3d] Added a link and a handler for the NoC\n", my_rank);
	    }

            // Create a channel for "out of band" events sent to ourselves
	    self_link= configureSelfLink("Me", new Event::Handler<Ghost_pattern>
		    (this, &Ghost_pattern::handle_self_events));
	    if (self_link == NULL)   {
		_ABORT(Ghost_pattern, "That was no good!\n");
	    } else   {
		_GHOST_PATTERN_DBG(2, "[%3d] Added a self link and a handler\n", my_rank);
	    }

            // Create a handler for events from local NVRAM
	    nvram= configureLink("NVRAM", new Event::Handler<Ghost_pattern>
		    (this, &Ghost_pattern::handle_nvram_events));
	    if (nvram == NULL)   {
		_GHOST_PATTERN_DBG(0, "The ghost pattern generator expects a link named \"NVRAM\"\n");
		_ABORT(Ghost_pattern, "Check the input XML file!\n");
	    } else   {
		_GHOST_PATTERN_DBG(2, "[%3d] Added a link and a handler for the NVRAM\n", my_rank);
	    }

            // Create a handler for events from the storage network
	    storage= configureLink("STORAGE", new Event::Handler<Ghost_pattern>
		    (this, &Ghost_pattern::handle_storage_events));
	    if (storage == NULL)   {
		_GHOST_PATTERN_DBG(0, "The ghost pattern generator expects a link named \"STORAGE\"\n");
		_ABORT(Ghost_pattern, "Check the input XML file!\n");
	    } else   {
		_GHOST_PATTERN_DBG(2, "[%3d] Added a link and a handler for the storage\n", my_rank);
	    }

	    self_link->setDefaultTimeBase(tc);
	    nvram->setDefaultTimeBase(tc);
	    storage->setDefaultTimeBase(tc);


	    // Initialize the common functions we need
	    common= new Patterns();
	    if (!common->init(x_dim, y_dim, NoC_x_dim, NoC_y_dim, my_rank, cores, net, self_link,
		    NoC, nvram, storage,
		    net_latency, net_bandwidth, node_latency, node_bandwidth,
		    chckpt_method, chckpt_size, chckpt_interval, envelope_size))   {
		_ABORT(Ghost_pattern, "Patterns->init() failed!\n");
	    }

	    // Who are my four neighbors?
	    // The network is x_dim * NoC_x_dim by y_dim * NoC_y_dim
	    // The virtual network of the cores is
	    // (x_dim * NoC_x_dim * cores) * (y_dim * NoC_y_dim)
	    int logical_width= x_dim * NoC_x_dim * cores;
	    int logical_height= y_dim * NoC_y_dim;
	    int myX= my_rank % logical_width;
	    int myY= my_rank / logical_width;
	    if (my_rank == 0)   {
		printf("||| Arranging ranks as a %d * %d logical mesh\n",
		    logical_width, logical_height);
	    }

	    right= ((myX + 1) % (logical_width)) + (myY * (logical_width));
	    left= ((myX - 1 + (logical_width)) % (logical_width)) + (myY * (logical_width));
	    down= myX + ((myY + 1) % logical_height) * (logical_width);
	    up= myX + ((myY - 1 + logical_height) % logical_height) * (logical_width);

	    //fprintf(stderr, "{%2d:%2d,%2d, node %2d} right %2d, left %2d, up %2d, down %2d\n",
	    //my_rank, myX, myY, my_rank / (NoC_x_dim * NoC_y_dim * cores), right, left, up, down);

	    // If we are doing coordinated checkpointing, we will do that
	    // every time x timesteps have been computed.
	    // This is OK as long as the checkpoint interval is larger than a time
	    // step. In the other case we would need to quiesce, lets do that later
	    switch (chckpt_method)   {
		case CHCKPT_NONE:
		    break;
		case CHCKPT_COORD:
		    if (chckpt_interval >= compute_time)   {
			chckpt_steps= chckpt_interval / compute_time;
			if (my_rank == 0)   {
			    printf("||| Will checkpoint every %d timesteps = %.9f s, %d bytes\n",
				chckpt_steps,
				(double)chckpt_steps * (double)compute_time / 1000000000.0,
				chckpt_size);
			}
		    } else   {
			_ABORT(Ghost_pattern, "Can't handle checkpoint interval %lu < timestep %lu\n",
			    chckpt_interval, compute_time);
		    }
		    break;

		case CHCKPT_UNCOORD:
		    if (my_rank == 0)   {
			printf("||| Don't know yet what'll have to set for uncoordinated...\n");
		    }
		    break;

		case CHCKPT_RAID:
		    _ABORT(Ghost_pattern, "Can't handle checkpoint methods other than coord and none\n");
		    break;
	    }

	    // Ghost pattern specific info
	    if (my_rank == 0)   {
		printf("||| Each timestep will take %.9f s\n", (double)compute_time / 1000000000.0);
		printf("||| Total application time is %.9f s = %.0f timesteps\n",
		    (double)application_end_time / 1000000000.0,
		    ceil((double)application_end_time / (double)compute_time));
		printf("||| Ghost cell exchange message size is %d bytes\n", exchange_msg_len);
	    }

	    // Send a start event to ourselves without a delay
	    common->event_send(my_rank, START);

        }

    private:

        Ghost_pattern(const Ghost_pattern &c);
	void handle_events(CPUNicEvent *e);
	void handle_net_events(Event *sst_event);
	void handle_NoC_events(Event *sst_event);
	void handle_self_events(Event *sst_event);
	void handle_nvram_events(Event *sst_event);
	void handle_storage_events(Event *sst_event);
	Patterns *common;

	// Input paramters for simulation
	int my_rank;
	int cores;
	int x_dim;
	int y_dim;
	int NoC_x_dim;
	int NoC_y_dim;
	SimTime_t net_latency;
	SimTime_t net_bandwidth;
	SimTime_t node_latency;
	SimTime_t node_bandwidth;
	SimTime_t compute_time;
	SimTime_t application_end_time;
	int exchange_msg_len;
	int ghost_pattern_debug;
	chckpt_t chckpt_method;
	SimTime_t chckpt_interval;
	int chckpt_size;
	int envelope_size;
	SimTime_t msg_write_time;

	// Precomputed values
	int left, right, up, down;
	int chckpt_steps;

	// Keeping track of the simulation
	state_t state;
	int rcv_cnt;
	int save_ENVELOPE_cnt;
	bool application_done;
	int timestep_cnt;
	int timestep_needed;
	bool done_waiting;

	// Statistics; some of these may move to pattern_common
	int num_chckpts;
	int total_rcvs;

	// Keeping track of time
	SimTime_t compute_segment_start;	// Time when we last entered compute
	SimTime_t msg_wait_time_start;		// Time when we last entered wait
	SimTime_t chckpt_segment_start;		// Time when we last entered coord chckpt

	SimTime_t execution_time;		// Total time of simulation
	SimTime_t application_time_so_far;	// Total time in compute so far
	SimTime_t msg_wait_time;		// Total time in eait so far
	SimTime_t chckpt_time;			// Total checkpoint time so far


        Params_t params;
	Link *net;
	Link *self_link;
	Link *NoC;
	Link *nvram;
	Link *storage;
	TimeConverter *tc;

	// Some local functions we need
	void state_INIT(pattern_event_t event);
	void state_COMPUTE(pattern_event_t event);
	void state_WAIT(pattern_event_t event);
	void state_DONE(pattern_event_t event);
	void state_CHCKPT(pattern_event_t event);
	void state_SAVE_ENVELOPE(pattern_event_t event);
	void state_LOG_MSG1(pattern_event_t event);
	void state_LOG_MSG2(pattern_event_t event);
	void state_LOG_MSG3(pattern_event_t event);
	void state_LOG_MSG4(pattern_event_t event);

	void transition_to_COMPUTE(void);
	void transition_to_CHCKPT(void);
	void count_receives(void);


        friend class boost::serialization::access;
        template<class Archive>
        void serialize(Archive & ar, const unsigned int version )
        {
            ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Component);
	    ar & BOOST_SERIALIZATION_NVP(params);
	    ar & BOOST_SERIALIZATION_NVP(my_rank);
	    ar & BOOST_SERIALIZATION_NVP(cores);
	    ar & BOOST_SERIALIZATION_NVP(x_dim);
	    ar & BOOST_SERIALIZATION_NVP(y_dim);
	    ar & BOOST_SERIALIZATION_NVP(NoC_x_dim);
	    ar & BOOST_SERIALIZATION_NVP(NoC_y_dim);
	    ar & BOOST_SERIALIZATION_NVP(net_latency);
	    ar & BOOST_SERIALIZATION_NVP(net_bandwidth);
	    ar & BOOST_SERIALIZATION_NVP(node_latency);
	    ar & BOOST_SERIALIZATION_NVP(node_bandwidth);
	    ar & BOOST_SERIALIZATION_NVP(compute_time);
	    ar & BOOST_SERIALIZATION_NVP(done_waiting);
	    ar & BOOST_SERIALIZATION_NVP(compute_segment_start);
	    ar & BOOST_SERIALIZATION_NVP(application_end_time);
	    ar & BOOST_SERIALIZATION_NVP(exchange_msg_len);
	    ar & BOOST_SERIALIZATION_NVP(state);
	    ar & BOOST_SERIALIZATION_NVP(left);
	    ar & BOOST_SERIALIZATION_NVP(right);
	    ar & BOOST_SERIALIZATION_NVP(up);
	    ar & BOOST_SERIALIZATION_NVP(down);
	    ar & BOOST_SERIALIZATION_NVP(rcv_cnt);
	    ar & BOOST_SERIALIZATION_NVP(ghost_pattern_debug);
	    ar & BOOST_SERIALIZATION_NVP(application_done);
	    ar & BOOST_SERIALIZATION_NVP(timestep_cnt);
	    ar & BOOST_SERIALIZATION_NVP(timestep_needed);
	    ar & BOOST_SERIALIZATION_NVP(chckpt_method);
	    ar & BOOST_SERIALIZATION_NVP(chckpt_steps);
	    ar & BOOST_SERIALIZATION_NVP(chckpt_interval);
	    ar & BOOST_SERIALIZATION_NVP(chckpt_size);
	    ar & BOOST_SERIALIZATION_NVP(envelope_size);
	    ar & BOOST_SERIALIZATION_NVP(msg_wait_time_start);
	    ar & BOOST_SERIALIZATION_NVP(num_chckpts);
	    ar & BOOST_SERIALIZATION_NVP(total_rcvs);
	    ar & BOOST_SERIALIZATION_NVP(compute_segment_start);
	    ar & BOOST_SERIALIZATION_NVP(msg_wait_time_start);
	    ar & BOOST_SERIALIZATION_NVP(chckpt_segment_start);
	    ar & BOOST_SERIALIZATION_NVP(execution_time);
	    ar & BOOST_SERIALIZATION_NVP(application_time_so_far);
	    ar & BOOST_SERIALIZATION_NVP(msg_wait_time);
	    ar & BOOST_SERIALIZATION_NVP(chckpt_time);
	    ar & BOOST_SERIALIZATION_NVP(net);
	    ar & BOOST_SERIALIZATION_NVP(self_link);
	    ar & BOOST_SERIALIZATION_NVP(tc);
        }

        template<class Archive>
        friend void save_construct_data(Archive & ar,
                                        const Ghost_pattern * t,
                                        const unsigned int file_version)
        {
            _AR_DBG(Ghost_pattern,"\n");
            ComponentId_t     id     = t->getId();
            Params_t          params = t->params;
            ar << BOOST_SERIALIZATION_NVP(id);
            ar << BOOST_SERIALIZATION_NVP(params);
        }

        template<class Archive>
        friend void load_construct_data(Archive & ar,
                                        Ghost_pattern * t,
                                        const unsigned int file_version)
        {
            _AR_DBG(Ghost_pattern,"\n");
            ComponentId_t     id;
            Params_t          params;
            ar >> BOOST_SERIALIZATION_NVP(id);
            ar >> BOOST_SERIALIZATION_NVP(params);
            ::new(t)Ghost_pattern(id, params);
        }
};

#endif // _GHOST_PATTERN_H
