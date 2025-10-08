FROM alpine:3.22
RUN apk update
RUN apk upgrade

RUN rm -rf /run /tmp
RUN ln -s /dev/shm /run
RUN ln -s /dev/shm /tmp

RUN apk add nodejs
RUN apk add dnsdist
RUN apk add haproxy
RUN apk add openssl
RUN apk add ca-certificates ca-certificates-bundle

RUN addgroup -g 850 -S hsd
RUN adduser -u 850 -S -G hsd hsd

COPY hsd-8.0.0/hsd /usr/local/hsd/

COPY haproxy.cfg /etc/haproxy/
COPY inittab /etc/inittab
COPY root_cron /var/spool/cron/crontabs/root
COPY bin /usr/local/bin/

RUN rm -f /var/cache/apk/*
CMD [ "/sbin/init" ]
