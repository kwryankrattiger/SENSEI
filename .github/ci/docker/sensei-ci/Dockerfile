FROM fedora:33

COPY setup-user.sh /root/
RUN /root/setup-user.sh sensei && \
  rm /root/setup-user.sh

# Install phase
RUN dnf update -y --setopt=install_weak_deps=False

# Spack
ENV SPACK_ROOT=/opt/spack
ENV SPACK_PYTHON=/usr/bin/python3
ENV SPACK_VERSION=develop

COPY install_spack.sh /root/
RUN SPACK_ROOT=/opt/spack /root/install_spack.sh && \
  rm /root/install_spack.sh

COPY packages.yaml /opt/spack/etc/spack/packages.yaml
COPY modules.yaml /opt/spack/etc/spack/modules.yaml

# SENSEI
COPY install_sensei_deps.sh /root/
RUN /root/install_sensei_deps.sh

# Switch to sensei user
WORKDIR /home/sensei
USER sensei

# Setup modules and such
COPY .bash_profile /home/sensei/.bash_profile

# Clean up installing stuff
RUN dnf clean all

CMD ["bash", "--login"]
