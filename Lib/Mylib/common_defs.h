#ifndef COMDEFS_H
#define COMDEFS_H
#include "stm32f1xx.h"
#include "log.h"
#include "dwt.h"

//#define SEGGER_SYSVIEW  1
//#define TRACEALYZER 1

#ifdef SEGGER_SYSVIEW  
#include "SEGGER_SYSVIEW.h"
#define TRACE_START                SEGGER_SYSVIEW_Conf()
#define TRACE_ENTER_ISR            SEGGER_SYSVIEW_RecordEnterISR()
#define TRACE_EXIT_ISR             SEGGER_SYSVIEW_RecordExitISR()
#define TRACE_RECORD_ENTER(num)    SEGGER_SYSVIEW_RecordVoid(num)
#define TRACE_RECORD_EXIT(num)     SEGGER_SYSVIEW_RecordEndCall(num)
#define TRACE_PRINTF(fmt, args...) SEGGER_SYSVIEW_PrintfHost(fmt , ## args)

#endif

#ifdef TRACEALYZER
#define TRACE_START                vTraceEnable(TRC_START)
#define TRACE_ENTER_ISR            
#define TRACE_EXIT_ISR             
#define TRACE_RECORD_ENTER(num)    
#define TRACE_RECORD_EXIT(num)     
#define TRACE_PRINTF(fmt, args...) 
#endif

#define ALGN4 __attribute__ ((aligned (4)))
#define ALGN8 __attribute__ ((aligned (8)))
#define ALGN32 __attribute__ ((aligned (32)))



#endif