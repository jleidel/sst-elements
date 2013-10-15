
// Copyright 2011 Sandia Corporation. Under the terms                          
// of Contract DE-AC04-94AL85000 with Sandia Corporation, the U.S.             
// Government retains certain rights in this software.                         
//                                                                             
// Copyright (c) 2011, Sandia Corporation                                      
// All rights reserved.                                                        
//                                                                             
// This file is part of the SST software package. For license                  
// information, see the LICENSE file in the top level directory of the         
// distribution.                                                               

#include "sst_config.h"
#include "ConstraintAllocator.h"

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

#include <boost/tokenizer.hpp>

#include "sst/core/serialization.h"

#include "AllocInfo.h"
#include "Job.h"
#include "Machine.h"
#include "misc.h"
#include "output.h"
#include "SimpleMachine.h"

#define DEBUG false

using namespace SST::Scheduler;

ConstraintAllocator::ConstraintAllocator(SimpleMachine* m, std::string DepsFile, std::string ConstFile) 
{
    schedout.init("", 8, 0, Output::STDOUT);
    machine = m;
    ConstraintsFileName = ConstFile;
    // read Dependencies
    // if file does not exist or is empty, D will be an empty mapping
    // and in effect we default to simple allocator
    std::ifstream DepsStream(DepsFile.c_str(),  std::ifstream::in );
    std::string u,v;
    std::string curline;
    std::stringstream lineStream;
    while (std::getline(DepsStream, curline)) { //for each line in file
        lineStream << curline;
        lineStream >> u; // line is u followed by elements of D[u]
        //if( DEBUG ) std::cout << "------------------Dependencies of " << u << std::endl;
        schedout.debug(CALL_INFO, 7, 0, "------------------Dependencies of %s", u.c_str());
        while (lineStream >> v) {
            D[u].insert(v);
            //if( DEBUG ) std::cout << v << " ";
            schedout.debug(CALL_INFO, 7, 0, "%s ", v.c_str());
        }
        if( DEBUG ) std::cout << std::endl;
        lineStream.clear(); //so we can write to it again
    }

    long int seed = 42;

    allocPRNGstate = (unsigned short *) malloc( 3 * sizeof( short ) );

    allocPRNGstate[ 0 ] = 0x330E;
    allocPRNGstate[ 1 ] = seed & 0xFFFF;
    allocPRNGstate[ 2 ] = seed >> 16;
}

//external process (python) will read analysis output and create a file
//which contains a cluster of nodes on each line
//if we cannot separate the first cluster we try the next et ceteral
//if file does not exist or is empty, Du and Dv will be empty sets
//an in effect we default to simple allocator
//of course this is inefficent, should check if file has changed
//instead of re-reading every time
void ConstraintAllocator::GetConstraints()
{
}

std::string ConstraintAllocator::getParamHelp()
{
    return "";
}

std::string ConstraintAllocator::getSetupInfo(bool comment)
{
    std::string com;
    if (comment) {
        com = "# ";
    } else { 
        com = "";
    }
    return com + "Constraint Allocator";
}

//allocates job if possible
//returns info on the allocation or NULL if it wasn't possible
AllocInfo* ConstraintAllocator::allocate(Job* job){
	AllocInfo * allocation = NULL;

	static int count = 0;

	std::vector<int> * freeProcs = ((SimpleMachine*)machine)->freeProcessors();

	if( (unsigned) job->getProcsNeeded() <= freeProcs->size() ){
		if( constraints_changed() ){
			read_constraints();
		}

		++ count;

		std::list<ConstrainedAllocation *> possible_allocations;

		for( std::list<std::set<std::string> * >::iterator constraint_iter = constraint_leaves.begin();
		     constraint_iter != constraint_leaves.end(); ++ constraint_iter ){
			ConstrainedAllocation * tmp = allocate_constrained( job, *constraint_iter ); 
			possible_allocations.push_back( tmp );
		}

		ConstrainedAllocation * top_allocation = get_top_allocation( possible_allocations );

		if( top_allocation != NULL ){
			allocation = generate_AllocInfo( top_allocation );
		}else{
			allocation = generate_RandomAllocInfo( job );
		}

		while( ! possible_allocations.empty() ){
			delete possible_allocations.back();
			possible_allocations.pop_back();
		}

	}

	delete freeProcs;

	return allocation;
}


AllocInfo * ConstraintAllocator::generate_RandomAllocInfo( Job * job ){
	AllocInfo * alloc = new AllocInfo( job );
	std::vector<int> * free_comp_nodes = ((SimpleMachine *)machine)->freeProcessors();

	for( int node_counter = 0; node_counter < job->getProcsNeeded(); node_counter ++ ){
		std::vector<int>::iterator node_iter = free_comp_nodes->begin();
		std::advance( node_iter, (nrand48( allocPRNGstate ) % free_comp_nodes->size()) );
		alloc->nodeIndices[ node_counter ] = *node_iter;
		free_comp_nodes->erase( node_iter );
	}
	
	delete free_comp_nodes;

	return alloc;
}


AllocInfo * ConstraintAllocator::generate_AllocInfo( ConstrainedAllocation * constrained_alloc ){
	AllocInfo * alloc = new AllocInfo( constrained_alloc->job );

	int node_counter = 0;

	for( std::set<int>::iterator unconstrained_node_iter = constrained_alloc->unconstrained_nodes.begin();
	     unconstrained_node_iter != constrained_alloc->unconstrained_nodes.end(); ++ unconstrained_node_iter ){
		alloc->nodeIndices[ node_counter ] = *unconstrained_node_iter;
		++ node_counter;
	}

	for( std::set<int>::iterator constrained_node_iter = constrained_alloc->constrained_nodes.begin();
	     constrained_node_iter != constrained_alloc->constrained_nodes.end(); ++ constrained_node_iter ){
		alloc->nodeIndices[ node_counter ] = *constrained_node_iter;
		++ node_counter;
	}

	return alloc;
}


ConstrainedAllocation * ConstraintAllocator::get_top_allocation( std::list<ConstrainedAllocation *> possible_allocations ){
	ConstrainedAllocation * top_allocation = NULL;

	for( std::list<ConstrainedAllocation *>::iterator allocation_iter = possible_allocations.begin();
	     allocation_iter != possible_allocations.end(); ++ allocation_iter ){
		if( top_allocation == NULL or
		    top_allocation->constrained_nodes.size() > (*allocation_iter)->constrained_nodes.size() ){
			top_allocation = *allocation_iter;
		}
	}

	return top_allocation;
}


bool ConstraintAllocator::constraints_changed(){
	return true;
}


void ConstraintAllocator::read_constraints(){
	boost::char_separator<char> space_separator( " " );
	std::ifstream ConstraintsStream(ConstraintsFileName.c_str(), std::ifstream::in );

	while( !constraint_leaves.empty() ){
		delete constraint_leaves.back();
		constraint_leaves.pop_back();
	}

	while(!ConstraintsStream.eof() and ConstraintsStream.is_open()){
		std::string curline;
		std::vector<std::string> CurrentCluster;

		getline(ConstraintsStream, curline);
		boost::tokenizer< boost::char_separator<char> > tok( curline, space_separator );
		for (boost::tokenizer< boost::char_separator<char> >::iterator iter = tok.begin(); iter != tok.end(); ++iter) {
			CurrentCluster.push_back(*iter);
		}

		this->constraint_leaves.push_back( get_constrained_leaves( CurrentCluster ) );
	}

	ConstraintsStream.close();
}


std::set< std::string > * ConstraintAllocator::get_constrained_leaves( std::vector<std::string> constraint ){
	std::set< std::string > * leaves = new std::set<std::string>;

	for( std::vector<std::string>::iterator constraint_iter = constraint.begin();
	     constraint_iter != constraint.end(); ++ constraint_iter ){
		std::set<std::string> constraint_children = D[ *constraint_iter ];
		for( std::set<std::string>::iterator constraint_child_iter = constraint_children.begin();
		     constraint_child_iter != constraint_children.end(); ++ constraint_child_iter ){
			if( 1 == D[ *constraint_child_iter ].size() ){
				leaves->insert( *constraint_child_iter );
			}
		}
	}

	return leaves;
}


//allocates job if possible
//returns information on the allocation or null if it wasn't possible
ConstrainedAllocation * ConstraintAllocator::allocate_constrained(Job* job, std::set<std::string> * constrained_leaves ){
	std::vector<int> * free_comp_nodes = ((SimpleMachine *)machine)->freeProcessors();

	std::set<int> free_constrained_nodes;
	std::set<int> free_unconstrained_nodes;

	int nodes_needed = job->getProcsNeeded();

	ConstrainedAllocation * new_allocation = new ConstrainedAllocation();
	new_allocation->job = job;

	for( std::vector<int>::iterator comp_node_iter = free_comp_nodes->begin();
	     comp_node_iter != free_comp_nodes->end(); ++ comp_node_iter ){
		if( constrained_leaves->find( ((SimpleMachine*)machine)->getNodeID( *comp_node_iter ) ) !=
		    constrained_leaves->end() ){
			free_constrained_nodes.insert( *comp_node_iter );
		}else{
			free_unconstrained_nodes.insert( *comp_node_iter );
		}
	}

	int unconstrained_nodes_needed = std::min( nodes_needed - 1, (int) free_unconstrained_nodes.size() );
	int constrained_nodes_needed = std::min( nodes_needed - unconstrained_nodes_needed, (int) free_constrained_nodes.size() );
	unconstrained_nodes_needed = nodes_needed - constrained_nodes_needed;

	while( unconstrained_nodes_needed > 0 ){
		std::set<int>::iterator unconstrained_node_iter = free_unconstrained_nodes.begin();
		std::advance( unconstrained_node_iter, (nrand48( allocPRNGstate ) % free_unconstrained_nodes.size()) );
		new_allocation->unconstrained_nodes.insert( *unconstrained_node_iter );
		free_unconstrained_nodes.erase( unconstrained_node_iter );
		-- unconstrained_nodes_needed;
	}

	while( constrained_nodes_needed > 0 ){
		std::set<int>::iterator constrained_node_iter = free_constrained_nodes.begin();
		std::advance( constrained_node_iter, (nrand48( allocPRNGstate ) % free_constrained_nodes.size()) );
		new_allocation->constrained_nodes.insert( *constrained_node_iter );
		free_constrained_nodes.erase( constrained_node_iter );
		-- constrained_nodes_needed;
	}

	delete free_comp_nodes;

	return new_allocation;
}
