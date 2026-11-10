# SPDX-License-Identifier: GPL-2.0-or-later
# Reference build environment; build context is the repository root.
FROM ubuntu:24.04@sha256:fe81560990fae86dc4743202b2cb4d3df98d0bcf949173cecef15ce1127ef852
COPY docs/validation/linux-packages.lock /tmp/linux-packages.lock
RUN apt-get update \
    && xargs -r apt-get install -y --no-install-recommends < /tmp/linux-packages.lock \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
CMD ["bash", "scripts/foliq/build-linux.sh"]
