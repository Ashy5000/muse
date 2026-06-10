#ifndef MSR_H
#define MSR_H

void setMSR(uint32_t msr, uint32_t lo, uint32_t hi);
void getMSR(uint32_t msr, uint32_t *lo, uint32_t *hi);


#endif
