FROM alpine:3.22

RUN apk update
RUN apk upgrade

RUN apk add nodejs

COPY hsd-8.0.0/hsd /usr/local/hsd/
COPY bin /usr/local/bin/

COPY inittab /etc/inittab

CMD [ "/sbin/init" ]
