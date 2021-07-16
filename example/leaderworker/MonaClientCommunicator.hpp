/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __COLZA_MONA_CLIENT_COMMUNICATOR_HPP
#define __COLZA_MONA_CLIENT_COMMUNICATOR_HPP

#include <colza/ClientCommunicator.hpp>
#include <mona-coll.h>
#include <mona.h>
// this is the patch for the colz things
// if the client is elastic, then we use the mona instead of MPI
namespace colza
{

class MonaClientCommunicator : public ClientCommunicator
{

  mona_comm_t m_comm;

public:
  MonaClientCommunicator(mona_comm_t comm)
    : m_comm(comm)
  {
  }

  ~MonaClientCommunicator() = default;

  int size() const override
  {
    int s;
    mona_comm_size(m_comm, &s);

    return s;
  }

  int rank() const override
  {
    int r;
    mona_comm_rank(m_comm, &r);

    return r;
  }

  void barrier() const override
  {
    int tag = 2022;
    mona_comm_barrier(m_comm, tag);
  }

  void bcast(void* buffer, int bytes, int root) const override
  {
    int tag = 2023;
    mona_comm_bcast(m_comm, buffer, bytes, root, tag);
  }
};

}

#endif
