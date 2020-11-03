#ifndef _ICET_COLZA_COMM_H
#define _ICET_COLZA_COMM_H

#include <IceT.h>
#include <colza/communicator.hpp>

IceTCommunicator icetCreateColzaCommunicator(const std::shared_ptr<colza::communicator>& colza_comm);

void icetDestroyColzaCommunicator(IceTCommunicator comm);

#endif
