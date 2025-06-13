
// bot.cpp
#include <iostream>
#include <string>
#include <sstream>
#include <sqlite3.h>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <unistd.h>

using json = nlohmann::json;
std::string BOT_TOKEN = "7938255031:AAFTgaIhwkLIABcK4c-kmjIxI0DWwdWeX1o";
std::string BASE_URL = "https://api.telegram.org/bot" + BOT_TOKEN + "/";

sqlite3 *db;

size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *output) {
    output->append((char *)contents, size * nmemb);
    return size * nmemb;
}

std::string sendRequest(const std::string& method, const std::string& params) {
    CURL *curl = curl_easy_init();
    std::string response;
    if(curl) {
        std::string url = BASE_URL + method + "?" + params;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
    return response;
}

void sendMessage(long chat_id, const std::string& text) {
    std::stringstream params;
    params << "chat_id=" << chat_id << "&text=" << curl_easy_escape(NULL, text.c_str(), text.length());
    sendRequest("sendMessage", params.str());
}

void initDB() {
    int rc = sqlite3_open("clients.db", &db);
    if(rc) {
        std::cerr << "Can't open database\n";
        exit(1);
    }
    const char *sql = "CREATE TABLE IF NOT EXISTS clients (id TEXT PRIMARY KEY);";
    sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
}

bool clientExists(const std::string& id) {
    std::string sql = "SELECT 1 FROM clients WHERE id='" + id + "' LIMIT 1;";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    int rc = sqlite3_step(stmt);
    bool exists = (rc == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return exists;
}

void addClient(const std::string& id) {
    std::string sql = "INSERT OR IGNORE INTO clients (id) VALUES('" + id + "');";
    sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
}

std::string getAllClients() {
    std::string result;
    const char *sql = "SELECT id FROM clients;";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    while(sqlite3_step(stmt) == SQLITE_ROW) {
        result += std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))) + "\n";
    }
    sqlite3_finalize(stmt);
    return result.empty() ? "База пуста." : result;
}

void handleUpdate(const json& update) {
    if(!update.contains("message")) return;
    auto message = update["message"];
    if(!message.contains("text")) return;

    std::string text = message["text"];
    long chat_id = message["chat"]["id"];

    if(text == "/start") {
        sendMessage(chat_id, "Привет! Используй /check <id>, /addclient <id> или /list");
    } else if(text.rfind("/check ", 0) == 0) {
        std::string id = text.substr(7);
        if(clientExists(id))
            sendMessage(chat_id, "Клиент найден в базе.");
        else
            sendMessage(chat_id, "Клиент не найден.");
    } else if(text.rfind("/addclient ", 0) == 0) {
        std::string id = text.substr(11);
        addClient(id);
        sendMessage(chat_id, "Клиент добавлен.");
    } else if(text == "/list") {
        sendMessage(chat_id, getAllClients());
    } else {
        sendMessage(chat_id, "Неизвестная команда.");
    }
}

int main() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    initDB();

    long last_update_id = 0;
    while(true) {
        std::stringstream params;
        params << "offset=" << last_update_id + 1;
        std::string response = sendRequest("getUpdates", params.str());

        try {
            auto j = json::parse(response);
            for(auto& upd : j["result"]) {
                last_update_id = upd["update_id"];
                handleUpdate(upd);
            }
        } catch(...) {
            std::cerr << "Ошибка парсинга JSON\n";
        }
        sleep(2);
    }

    sqlite3_close(db);
    curl_global_cleanup();
    return 0;
}
