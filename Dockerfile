FROM zevarito/ubuntu-base:latest

MAINTAINER Alvaro Gil

ARG BRANCH

WORKDIR /opt/licode

# Replace shell with bash so we can source files
RUN rm /bin/sh && ln -s /bin/bash /bin/sh

# Install NVM
RUN echo "stable" > .nvmrc
ADD .nvmrc ~/.nvmrc
RUN curl https://raw.githubusercontent.com/creationix/nvm/v0.30.1/install.sh | bash
RUN source ~/.nvm/nvm.sh; \
    nvm install $NODE_VERSION; \
    nvm use --delete-prefix $NODE_VERSION;

ADD . .
RUN ./scripts/installUbuntuDeps.sh --cleanup
RUN ./nuve/installNuve.sh
RUN ./scripts/installErizo.sh

ENV LICODE_ROOT /opt/licode
ENV PATH $PATH:/usr/local/sbin
ENV LD_LIBRARY_PATH $LD_LIBRARY_PATH:$LICODE_ROOT/erizo/build/erizo:$LICODE_ROOT/erizo:$ROOT/build/libdeps/build/lib
ENV ERIZO_HOME $LICODE_ROOT/erizo/
