#include <mpi.h>
#include <vector>
#include <icet/mona.hpp>

class Mandelbulb;

namespace PController
{

void processJoin(MPI_Comm subcomm, int globalrank, int localnprocs, int& color,
  int total_block_number, std::vector<Mandelbulb>& MandelbulbList);

void processJoin(mona_comm_t subcomm, mona_comm_t boostrapcomm, int globalrank, int localnprocs,
  int& color, int total_block_number, std::vector<Mandelbulb>& MandelbulbList);

void processLeave(MPI_Comm subcomm, int localrank, int localnprocs, int& color,
  int total_block_number, std::vector<Mandelbulb>& MandelbulbList, int process_leave_num,
  int baseoffset);

void processLeave(mona_comm_t subcomm, int localrank, int localnprocs, int& color,
  int total_block_number, std::vector<Mandelbulb>& MandelbulbList, int process_leave_num,
  int baseoffset);

} // namespace PController