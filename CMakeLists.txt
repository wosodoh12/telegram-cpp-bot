
cmake_minimum_required(VERSION 3.10)
project(TelegramBot)

set(CMAKE_CXX_STANDARD 17)
find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)

add_executable(bot bot.cpp)
target_link_libraries(bot CURL::libcurl SQLite::SQLite3)
