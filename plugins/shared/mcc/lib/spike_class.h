#ifndef SELECTA_MCC_SPIKE_CLASS_H
#define SELECTA_MCC_SPIKE_CLASS_H

#include <exec/types.h>

/* The dummy Area subclass of the I685b spike: fills its rectangle with one pen, fixed minimum size. */
extern ULONG spike_dispatch(void);
ULONG spike_instance_size(void);

#endif
