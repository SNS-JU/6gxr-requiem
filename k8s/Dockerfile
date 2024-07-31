### BUILDER
FROM  ubuntu:22.04 AS builder

ENV TZ=Europe/Budapest

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y \
     tzdata \
     python3 \
     python3-pip \
     ninja-build \
     libsoup2.4-1 \
     libsoup2.4-dev \
     libssl3 \
     libssl-dev \
     libsrtp2-1 \
     libsrtp2-dev \
     libgnutls30 \
     libgnutls28-dev \
     libgnutls-openssl27 \
     libnice10 \
     libnice-dev \
     libwebsockets16 \
     libwebsockets-dev \
     flex \
     bison \
     libpango-1.0-0 \
     libpango1.0-dev \
     libpango-1.0-0 \
     libpango1.0-dev \
     libpangocairo-1.0-0 \
     libjson-glib-1.0-0 \
     libjson-glib-dev \
     libavfilter7 \
     libavfilter-dev \
     libavformat58 \
     libavformat-dev \
     libavcodec58 \
     libavcodec-dev \
     libavutil56 \
     libavutil-dev \
     libgtk-3-dev \
     libtool \
     git \
     libusrsctp2 \
     curl

RUN pip3 install --no-cache-dir -Iv meson==0.62

WORKDIR /src
RUN update-ca-certificates \
    && git config --global http.sslverify false \
    && git clone --depth=1 --branch 1.22 \
                 https://gitlab.freedesktop.org/gstreamer/gstreamer.git

WORKDIR gstreamer
RUN mkdir build \
    && meson --prefix=/usr build \
        -Dgpl=enabled \
        -Drs=disabled \
        -Dlibav=enabled \
        -Dgst-plugins-ugly:x264=enabled \
        -Dgst-plugins-base:pango=enabled \
        -Ddevtools=disabled \
        -Dtests=disabled \
        -Dexamples=disabled \
        -Ddoc=disabled \
        -Dorc=disabled \
        -Dlibsoup:sysprof=disabled \
        -Dbuildtype=release \
    && ninja -C build \
    && meson install -C build


# ------------ SOME PACK GSTREAMER RUST --------------------------------
ENV RUSTUP_INIT_SKIP_PATH_CHECK=yes
ENV PATH=/root/.cargo/bin:$PATH

RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
RUN cargo install cargo-c

WORKDIR /src
RUN git clone --depth 1 --branch pre-validation \
    https://github.com/SNS-JU/6gxr-quinn.git

RUN git clone --depth 1 --branch quic \
    https://github.com/SNS-JU/6gxr-gst-plugins-rs.git

WORKDIR /src/gst-plugins-rs/net/quinn
RUN cargo add quinn --path ../../../quinn/quinn \
    && cargo add quinn-proto --path ../../../quinn/quinn-proto

WORKDIR /src/gst-plugins-rs

RUN for name in fmp4 quinn; do \
     cargo cbuild -p gst-plugin-$name --prefix=/usr --libdir=/usr/lib/x86_64-linux-gnu --release \
     && cargo cinstall -p gst-plugin-$name --prefix=/usr --libdir=/usr/lib/x86_64-linux-gnu; \
    done

RUN cargo uninstall cargo-c && \
    cargo clean

WORKDIR /
RUN git clone --depth 1 \
          https://github.com/SNS-JU/6gxr-requiem \
    && cd requiem/gst-pipeline \
    && make

RUN apt-get remove -y --purge git build-essential \
    && apt-get autoremove -y \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* /src /tmp/workdir


### FINAL
FROM  ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y \
        libblas3 \
	liblapack3 \
	tcpdump \
	tar \
	bc \
    && apt-get autoremove -y \
    && apt-get clean

COPY --from=builder /usr/bin /usr/bin/
COPY --from=builder /usr/lib /usr/lib/
COPY --from=builder /usr/libexec /usr/libexec/

COPY --from=builder /requiem /measure

CMD exec /bin/bash -c "trap : TERM INT; sleep infinity & wait"
