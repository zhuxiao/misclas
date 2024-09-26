# Use Ubuntu as the base image
FROM ubuntu

# Install necessary packages
RUN apt-get update && apt-get install -y \
    autoconf \
    automake \
    g++ \
    gcc \
    git \
    libbz2-dev\
    libcurl4-gnutls-dev \
    liblzma-dev \
    libssl-dev \
    libtool \
    make \
    mlpack-bin \
    perl \
    python3 \
    zlib1g-dev

# Download and install htslib from source
RUN git clone https://github.com/samtools/htslib.git && \
    cd htslib && \
    git submodule update --init --recursive && \
    autoreconf -i && \
    ./configure && \
    make && \
    make install

# Add htslib to the library path
ENV LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:/htslib"

# Use local misclas (to allow dev edits)
ADD . /misclas
RUN cd misclas && \
    bash autogen.sh

ENV PATH="/misclas/:/misclas/bin:${PATH}"

# Set the entry point or run any desired command
ENTRYPOINT ["/misclas/misclas.py"]
