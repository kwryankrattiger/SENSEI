set -e
set -x

source /root/.bashrc

# These are required for running sensei tests
dnf install -y --setopt=install_weak_deps=False \
  vim less bc

# Install main dependencies with spack
source ${SPACK_ROOT}/share/spack/setup-env.sh

# Installing Git because OpenSSL conflict with
# system breaks the system Git when modules are loaded
spack install -v -j$(grep -c '^processor' /proc/cpuinfo) git

# Install SENSEI dependencies
spack install -v -j$(grep -c '^processor' /proc/cpuinfo) --only dependencies \
  sensei

# Install packages to an environment
spack env create --without-view ci
spack -e ci add $(spack find --format "/{hash}")
spack -e ci install

# Delete the spack root
rm -rf /root/.spack/

spack env activate ci
spack env deactivate ci

spack -e ci env loads

spack clean -a
