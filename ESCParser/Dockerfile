FROM alpine:3.11 AS stage

COPY . /build

WORKDIR /build

RUN apk --no-cache add gcc g++ make libc-dev libstdc++
RUN make clean && make

FROM alpine:3.11

COPY --from=stage /build/ESCParser /bin

RUN apk --no-cache add libstdc++ libgcc

WORKDIR /work

ENTRYPOINT [ "/bin/ESCParser" ]
