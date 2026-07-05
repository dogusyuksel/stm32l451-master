#ifndef __LIBCANARD_HANDLERS_H__
#define __LIBCANARD_HANDLERS_H__

#ifndef USE_CSP_OVER_CANARD

#define CANARD_SPIN_PERIOD 500
#define LIBCANARD_LOOP_DELAY 100
#define LOCAL_NODE_ID 10

void uavcanInit(void);
void sendCanard(void);
void receiveCanard(void);
void spinCanard(void);
void libcanard_task_start(void *argument);

#endif

#endif // __LIBCANARD_HANDLERS_H__
