echo "Initializing Spack"
source ${SPACK_ROOT}/share/spack/setup-env.sh

echo "Initializing modules"
source /usr/share/lmod/lmod/init/bash

echo "Loading Module Environment"
spack env activate ci
spack env deactivate
module use ${SPACK_ROOT}/share/spack/modules/linux-fedora33-haswell
source ${SPACK_ROOT}/var/spack/environments/sensei/loads
