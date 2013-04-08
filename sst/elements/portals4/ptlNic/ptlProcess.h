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

#include "ptlNic.h"

namespace SST {
namespace Portals4 {

class GetIdCmd : public PtlNic::Cmd 
{
    enum { ArgId };
    enum { WriteId, WriteIdWait } m_state;
  public:
    GetIdCmd( PtlNic& nic, Context& ctx, PtlNicEvent* e ) :
        PtlNic::Cmd(nic,ctx,e),
        m_state( WriteId )
    { 
        PRINT_AT(PtlCmd," &id=%#lx \n", m_event->args[ArgId]);
    }
    bool work() {
        switch ( m_state ) {
            
          case WriteId:
            if ( writeId() != Finished ) break;
            m_state = WriteIdWait;

          case WriteIdWait:
            if ( writeIdWait() != Finished ) break;
        
        }
        return m_done;
    }
  private: 

    State writeId(){
        m_dmaCompletion = new Callback< GetIdCmd >
                                        ( this, &GetIdCmd::dmaDone );
        ptl_process_t* id = m_ctx.id();
        m_nic.dmaEngine().write( m_event->args[ArgId], 
                                (uint8_t*) id,
                                sizeof( *id ),
                                m_dmaCompletion );
        return Finished;
    }

    State writeIdWait(){
        if ( ! m_dmaCompletion->done ) return NotFinished;
        m_retval = PTL_OK;
        m_done = true;
        delete m_dmaCompletion; 
        return Finished;
    }

    bool dmaDone( void* ) {
        PRINT_AT(PtlCmd,"\n");
        m_dmaCompletion->done = true;
        return false;
    }

    Callback< GetIdCmd >*  m_dmaCompletion;
};

}
}
