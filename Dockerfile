FROM zevarito/ubuntu-base:latest

MAINTAINER Alvaro Gil

ARG BRANCH

WORKDIR /opt/licode

RUN apt-get update --fix-missing
# yikes installNuve requiere this :(
RUN apt-get install -y default-jdk

RUN rm /bin/sh && ln -s /bin/bash /bin/sh

# Install NVM
ENV NVM_DIR /opt/licode/build/libdeps/nvm
ENV NODE_VERSION v6.9.2
RUN curl https://raw.githubusercontent.com/creationix/nvm/v0.30.1/install.sh | bash
RUN source $NVM_DIR/nvm.sh \
    && nvm install $NODE_VERSION \
    && nvm alias default $NODE_VERSION \
    && nvm use default
ENV NODE_PATH $NVM_DIR/$NODE_VERSION/lib/node_modules
ENV PATH $NVM_DIR/versions/node/$NODE_VERSION/bin:$PATH

ADD . .
RUN bash -l -c "./scripts/installUbuntuDeps.sh --cleanup"
RUN bash -l -c "./nuve/installNuve.sh"
RUN bash -l -c "./scripts/installErizo.sh"

ENV LICODE_ROOT /opt/licode
ENV PATH $PATH:/usr/local/sbin
ENV LD_LIBRARY_PATH $LD_LIBRARY_PATH:$LICODE_ROOT/erizo/build/erizo:$LICODE_ROOT/erizo:$ROOT/build/libdeps/build/lib
ENV ERIZO_HOME $LICODE_ROOT/erizo/
