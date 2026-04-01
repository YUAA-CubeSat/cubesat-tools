#ifndef _EXECUTORS_H
#define _EXECUTORS_H

#include "telcomparse.h"

#define X(enumid, name, id) int cmdexecutor_##name(void);
CMD_LIST
#undef X

#endif