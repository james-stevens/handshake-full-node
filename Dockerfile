FROM alpine:3.22
RUN apk update
RUN apk upgrade

RUN rm -rf /run /tmp
RUN ln -s /dev/shm /run
RUN ln -s /dev/shm /tmp
RUN mkdir /usr/local/etc

RUN apk add nodejs

RUN addgroup -g 850 -S hsd
RUN adduser -u 850 -S -G hsd hsd

COPY hsd-8.0.0/hsd /usr/local/hsd/

COPY inittab /etc/inittab
COPY root_cron /var/spool/cron/crontabs/root
COPY bin /usr/local/bin/

RUN rm -f /var/cache/apk/*
RUN /usr/local/bin/make_build
CMD [ "/sbin/init" ]
