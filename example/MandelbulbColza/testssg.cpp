#include <colza/MPIClientCommunicator.hpp>
#include <fstream>
#include <iostream>
#include <mpi.h>
#include <spdlog/spdlog.h>
#include <ssg-mpi.h>
#include <ssg.h>
#include <tclap/CmdLine.h>
#include <thallium.hpp>

#ifdef USE_GNI
extern "C"
{
#include <rdmacred.h>
}
#include <margo.h>
#include <mercury.h>
#define DIE_IF(cond_expr, err_fmt, ...)                                                            \
  do                                                                                               \
  {                                                                                                \
    if (cond_expr)                                                                                 \
    {                                                                                              \
      fprintf(stderr, "ERROR at %s:%d (" #cond_expr "): " err_fmt "\n", __FILE__, __LINE__,        \
        ##__VA_ARGS__);                                                                            \
      exit(1);                                                                                     \
    }                                                                                              \
  } while (0)
#endif

const std::string credFileName = "global_cred_conf";
namespace tl = thallium;

int main(int argc, char** argv)
{

  MPI_Init(&argc, &argv);

  ssg_init();

  colza::MPIClientCommunicator comm(MPI_COMM_WORLD);
  int rank = comm.rank();
  int nprocs = comm.size();

  int ret;
#ifdef USE_GNI
  drc_info_handle_t drc_credential_info;
  uint32_t drc_cookie;
  char drc_key_str[256] = { 0 };
  struct hg_init_info hii;
  memset(&hii, 0, sizeof(hii));
  uint32_t drc_credential_id;
  if (rank == 0)
  {
    std::ifstream infile(credFileName);
    std::string cred_id;
    std::getline(infile, cred_id);
    std::cout << "load cred_id: " << cred_id << std::endl;
    drc_credential_id = (uint32_t)atoi(cred_id.c_str());
  }
  MPI_Bcast(&drc_credential_id, 1, MPI_UINT32_T, 0, MPI_COMM_WORLD);

  /*
  int ret = ssg_group_id_load(g_ssg_file.c_str(), &num_addrs, &g_id);
  DIE_IF(ret != SSG_SUCCESS, "ssg_group_id_load");
  if (rank == 0)
  {
    std::cout << "get num_addrs: " << num_addrs << std::endl;
  }

  int64_t ssg_cred = ssg_group_id_get_cred(g_id);
  DIE_IF(ssg_cred == -1, "ssg_group_id_get_cred");
  */

  /* access credential and covert to string for use by mercury */
  ret = drc_access(drc_credential_id, 0, &drc_credential_info);
  DIE_IF(ret != DRC_SUCCESS, "drc_access %u %ld", drc_credential_id);
  drc_cookie = drc_get_first_cookie(drc_credential_info);
  sprintf(drc_key_str, "%u", drc_cookie);
  hii.na_init_info.auth_key = drc_key_str;

  margo_instance_id mid = margo_init_opt("gni", MARGO_CLIENT_MODE, &hii, true, 2);
  tl::engine engine(mid);

#else
  // Initialize the thallium server
  tl::engine engine("tcp", THALLIUM_SERVER_MODE);

#endif

  ssg_group_id_t gid = SSG_GROUP_ID_INVALID;
  int num_addrs = SSG_ALL_MEMBERS;
  std::string ssg_group_file = "ssgfile";
  ret = ssg_group_id_load(ssg_group_file.c_str(), &num_addrs, &gid);
  if (ret != SSG_SUCCESS)
    throw std::runtime_error("Could not open SSG group file");
  auto midengine = engine.get_margo_instance();
  ret = ssg_group_observe(midengine, gid);
  if (ret != SSG_SUCCESS)
    throw std::runtime_error("Could not observe the SSG group from file");
  // get string addresses
  int group_size = ssg_get_group_size(gid);
  std::cout << "group size: " << group_size << std::endl;

  // std::vector<char> packed_addresses(group_size * 256, 0);
  //ssg_group_dump(gid);
  for (int i = 0; i < group_size; i++)
  {
    /*
     if we do not add sleep, there is mercurey issue
      # na_ofi_addr_lookup(): Unrecognized provider type found from:
      unable to resolve address
    
    // tl::thread::sleep(engine, 1000);
    ssg_member_id_t member_id = ssg_get_group_member_id_from_rank(gid, i);
    std::cout << "get member id " << member_id << std::endl;
    hg_addr_t member_addr = ssg_get_group_member_addr(gid, member_id);
    char addr[1024];
    hg_size_t addr_size = 1024;
    margo_addr_to_string(mid, addr, &addr_size, member_addr);
    // check the return value of the above function and then printf addr
    //
    //   addresses can not be extracted completely in large scale case by this api
    // auto addr = ssg_group_id_get_addr_str(gid, i);
    // if (!addr)
    //  throw std::runtime_error("Could not get address");
    std::cout << "get addr: " << std::string(addr) << std::endl;
    // strcpy(packed_addresses.data() + i * 256, addr);
    */
  }

  return 0;
}
