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
RUN pacman -S --noconfirm fakeroot base-devel sudo nano wget

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

RUN sudo pacman -S --noconfirm libxinerama readline bash perl libxtst libxft texinfo libxrandr gnupg

#Build Ratpoison pkg manually, it's not available in arch anymore
RUN mkdir /src/ratpoison  && \
    wget https://aur.archlinux.org/cgit/aur.git/plain/PKGBUILD?h=ratpoison -O /src/ratpoison/PKGBUILD && \
    wget https://aur.archlinux.org/cgit/aur.git/plain/ratpoison.desktop?h=ratpoison -O /src/ratpoison/ratpoison.desktop && \
    gpg --recv-keys 0DFA74AE1524E7EE && \
    cd /src/ratpoison && \
    sudo -u calaos makepkg -s

FROM archlinux:base as runner

# Set TERM environment variable
ENV TERM=xterm-color

#required packages
RUN pacman-key --init
RUN pacman-key --populate archlinux
RUN pacman -Syu --noconfirm

COPY --from=builder /src/pkgbuilds/calaos-home/*pkg.tar.zst* /
COPY --from=builder /src/ratpoison/*pkg.tar.zst* /

RUN pacman -U --noconfirm /*pkg.tar.zst*

RUN rm -fr /*pkg.tar.zst*

RUN pacman -S --noconfirm \
    xorg-server mesa \
    xf86-input-{libinput,evdev,elographics,synaptics,vmmouse} \
    xf86-video-{amdgpu,ati,fbdev,intel,nouveau,vesa,vmware} \
    xorg-setxkbmap virtualbox-guest-utils \
    xorg-{server,xinit,apps} xdg-user-dirs \
    ttf-{bitstream-vera,liberation,freefont,dejavu} freetype2 \
    xterm tslib

COPY start.sh /usr/bin
COPY ratpoisonrc /root/.ratpoisonrc
COPY xinitrc /root/.xinitrc