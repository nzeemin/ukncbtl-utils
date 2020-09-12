FROM alpine:latest AS stage

COPY . /build

WORKDIR /build

RUN apk update && apk add gcc g++ make libc-dev libstdc++
RUN make clean && make

FROM alpine:latest

COPY --from=stage /build/ESCParser /bin

RUN apk update && apk add libstdc++ libgcc

WORKDIR /work

ENTRYPOINT [ "/bin/ESCParser" ]
