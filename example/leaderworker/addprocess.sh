
#!/bin/sh
srun -C haswell -N 1 -l -n 1 -c 4 --cpu_bind=cores --time=10:00 ./example/leaderworker/testsim \
              -a gni \
              -v debug \
              -t 4 -j > elasticity_leaderworker_join_add$1.log 2>&1 &

# there is a delay of the log for new added process, how to let it start from the same iteration 
# with the simulation but not start from the scratch