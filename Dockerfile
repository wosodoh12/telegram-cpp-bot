
FROM ubuntu:20.04

RUN apt update && apt install -y \

    g++ cmake libcurl4-openssl-dev libsqlite3-dev nlohmann-json3-dev \

    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN cmake . && make

CMD ["./bot"]
