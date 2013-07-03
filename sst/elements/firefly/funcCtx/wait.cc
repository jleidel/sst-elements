// Copyright 2013 Sandia Corporation. Under the terms
// of Contract DE-AC04-94AL85000 with Sandia Corporation, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2013, Sandia Corporation
// All rights reserved.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.


#include <sst_config.h>
#include "sst/core/serialization/element.h"

#include "funcCtx/wait.h"
#include "hades.h"

#include <cxxabi.h>

#define DBGX( fmt, args... ) \
{\
    char* realname = abi::__cxa_demangle(typeid(*this).name(),0,0,NULL);\
    fprintf( stderr, "%s::%s():%d: "fmt, realname ? realname : "?????????", \
                        __func__, __LINE__, ##args);\
    if ( realname ) free(realname);\
}


using namespace SST::Firefly;
using namespace Hermes;

WaitCtx::WaitCtx( MessageRequest* req,  MessageResponse* resp, 
            Functor* retFunc, FunctionType type, Hades* obj ) : 
    FunctionCtx( retFunc, type, obj ),
    m_req( req ),
    m_resp( resp ),
    m_state( RunProgress )
{ }

bool WaitCtx::run( ) 
{
    bool retval = false;
    
    switch( m_state ) {
    case RunProgress:
        DBGX( "RunProgress\n" );
        m_obj->runProgress( this );
        m_state = Wait; 
        break;
    case Wait:
        DBGX("Wait\n");
        if ( m_req->src != AnySrc ) { 
            retval =  true;
            m_obj->clearIOCallback();
        } else {
            m_obj->setIOCallback();
        }
        break;
    }
    return retval;
}
