
FROM gcc:latest

RUN apt update && apt install -y curl libcurl4-openssl-dev sqlite3 libsqlite3-dev

COPY . /app
WORKDIR /app

RUN g++ -std=c++17 -o bot bot.cpp -lcurl -lsqlite3

CMD ["./bot"]
