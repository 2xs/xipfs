FROM alpine:3.24

RUN apk add --no-cache make gcc libc-dev

#RUN opam install rocq-core=9.1.1 rocq-stdlib=9.0.0\
# && opam clean -a -c -s --logs
