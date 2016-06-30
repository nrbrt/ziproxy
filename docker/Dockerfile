FROM debian:jessie
MAINTAINER shinobit <norklaus@gmail.com>
RUN apt-get update && apt-get install -y build-essential git zlib1g-dev libgif-dev libjpeg-dev libpng-dev libjasper-dev libsasl2-dev
RUN git clone --recursive https://github.com/nrbrt/ziproxy.git
RUN cd ziproxy && ./configure && make && make install
COPY startproxy /usr/local/bin/startproxy
RUN chmod +x /usr/local/bin/startproxy
RUN mkdir /var/log/ziproxy && mkdir /etc/ziproxy && touch /var/log/ziproxy/error.log
ENTRYPOINT ["/usr/local/bin/startproxy"]
