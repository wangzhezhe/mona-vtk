#ifndef _ICET_MONA_COMM_H
#define _ICET_MONA_COMM_H

#include <IceT.h>
#include <mona.h>
#include <mona-coll.h>

IceTCommunicator icetCreateMonaCommunicator(const mona_comm_t mona_comm);

void icetDestroyMonaCommunicator(IceTCommunicator comm);

#endif
