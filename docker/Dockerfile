FROM debian:bookworm as builder

ENV DEBIAN_FRONTEND noninteractive

ENV HOME /opt
RUN mkdir -p $HOME /build /opt

#required packages
RUN apt-get update -qq && \
    apt-get install -y build-essential wget git curl qt6-base-dev \
        qt6-5compat-dev qt6-declarative-dev qt6-svg-dev qt6-virtualkeyboard-dev \
        qt6-webchannel-dev qt6-websockets-dev qt6-webview-dev qt6-shadertools-dev \
        libxext-dev xorg-dev qt6-l10n-tools

RUN git clone https://github.com/calaos/calaos_mobile.git

RUN cd calaos_mobile && mkdir -p build && cd build && \
    qmake6 ../desktop.pro && \
    make -j$(nproc) PREFIX=/opt && \
    install -d /opt/bin && \
    install -m 0755 calaos_home /opt/bin/calaos_home

# Clean up APT when done.
RUN apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* /build/*

FROM debian:bookworm-slim as runner

RUN apt-get update -qq && \
    apt-get install -y xserver-xorg xkb-data xinit xfonts-base xfonts-scalable xfonts-75dpi xfonts-100dpi libxext6 \
        x11-xkb-utils x11-xserver-utils xauth \
        libqt6core6 libqt6gui6 \
        libqt6network6 libqt6qml6 \
        libqt6quick6 libqt6widget6 \
        qml6-module-qtquick-virtualkeyboard

COPY --from=builder /opt /opt
