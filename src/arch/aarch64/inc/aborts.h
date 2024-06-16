#ifndef ABORTS_H
#define ABPRTS_H

void aborts_sync_hanlder();
void aborts_data_lower(unsigned long iss, unsigned long far, unsigned long il, unsigned long ec);

#endif