
FROM gcc:latest

# Установка зависимостей
RUN apt update && apt install -y \
    curl \
    libcurl4-openssl-dev \
    sqlite3 \
    libsqlite3-dev \
    wget \
    unzip

# Установка nlohmann/json (только заголовочный файл)
RUN mkdir -p /usr/local/include/nlohmann && \
    wget -q https://github.com/nlohmann/json/releases/latest/download/json.hpp -O /usr/local/include/nlohmann/json.hpp

# Копируем проект
COPY . /app
WORKDIR /app

# Компиляция
RUN g++ -std=c++17 -o bot bot.cpp -lcurl -lsqlite3

# Запуск
CMD ["./bot"]
