
#!/bin/sh
srun -C haswell -N 1 -l -n 1 -c 4 --cpu_bind=cores --time=10:00 ./example/leaderworker/teststagingserver \
              -a gni \
              -v debug \
              -t 4 -j > elasticity_leaderworker_server_add$1.log 2>&1 &
