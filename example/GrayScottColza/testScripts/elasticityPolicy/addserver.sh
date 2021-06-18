
#!/bin/sh
# set -p 500 to avoid the ssg_apply_member_updates: Assertion `0' failed.
srun -C haswell -n 1 ./example/GrayScottColza/gsserver -j \
              -p 500 \
              -a ofi+gni \
              -s ssgfile \
              -v trace \
              -c /global/homes/z/zw241/cworkspace/src/mona-vtk/example/GrayScottColza/pipeline/gsMonaConfig.json \
              -t 1 > gsserver_monaelasticityAdd_$1.log 2>&1 &