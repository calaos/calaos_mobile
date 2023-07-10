FROM archlinux:base-devel as builder

ARG UID=1001
ARG GID=1001
ARG USER=calaos

ARG ARCH=x86_64
ARG COMMIT=
ARG PKGVERSION=

# Set TERM environment variable
ENV TERM=xterm-color

#required packages
RUN pacman-key --init
RUN pacman-key --populate archlinux
RUN pacman -Syu --noconfirm
RUN pacman -S --noconfirm git
RUN pacman -S --noconfirm fakeroot base-devel sudo nano

# Create user and its home
#RUN addgroup --gid ${GID} docker
RUN groupadd -g ${GID} docker
RUN useradd -d /home/${USER} -r -u ${UID} -g ${GID} ${USER}
RUN usermod -G wheel ${USER}
RUN mkdir -p -m 0755 /home/${USER}
RUN chown ${USER} /home/${USER}
RUN echo '%wheel ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

RUN echo 'PACKAGER="Calaos <raoul-arch@calaos.fr>"' >> /etc/makepkg.conf

USER ${USER}

# Define entry point
WORKDIR /src

RUN git clone https://github.com/calaos/calaos-build.git

RUN cd calaos-build && \
    scripts/get_pkgbuilds.sh && \
    scripts/build_pkg.sh "calaos-home" "" "$ARCH" "$COMMIT" "$PKGVERSION"

FROM archlinux:base as runner

# Set TERM environment variable
ENV TERM=xterm-color

#required packages
RUN pacman-key --init
RUN pacman-key --populate archlinux
RUN pacman -Syu --noconfirm

COPY --from=builder /src/pkgbuilds/calaos-home/*pkg.tar.zst* /

RUN pacman -U --noconfirm /*pkg.tar.zst*

RUN rm -fr /*pkg.tar.zst*

COPY start.sh /usr/bin
