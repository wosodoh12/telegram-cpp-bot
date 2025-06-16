
// bot.cpp
#include <iostream>
#include <string>
#include <sstream>
#include <sqlite3.h>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <unistd.h>
#include <ctime>

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
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
    return response;
}

void sendMessageWithButtons(long chat_id, const std::string& text) {
    json keyboard = {
        {"inline_keyboard", {
            { {{"text", "Проверить ID"}, {"callback_data", "check"}} },
            { {{"text", "Добавить ID"}, {"callback_data", "add"}} },
            { {{"text", "Список клиентов"}, {"callback_data", "list"}} }
        }}
    };

    std::stringstream params;
    params << "chat_id=" << chat_id
           << "&text=" << curl_easy_escape(NULL, text.c_str(), text.length())
           << "&reply_markup=" << curl_easy_escape(NULL, keyboard.dump().c_str(), keyboard.dump().length());

    sendRequest("sendMessage", params.str());
}

void sendMessage(long chat_id, const std::string& text) {
    std::stringstream params;
    params << "chat_id=" << chat_id
           << "&text=" << curl_easy_escape(NULL, text.c_str(), text.length());
    sendRequest("sendMessage", params.str());
}

void initDB() {
    if(sqlite3_open("clients.db", &db)) {
        std::cerr << "Can't open database\n";
        exit(1);
    }
    const char *sql = R"(
        CREATE TABLE IF NOT EXISTS clients (
            id TEXT PRIMARY KEY,
            username TEXT,
            added_by TEXT,
            added_at TEXT
        );
    )";
    sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
}

std::string checkOrAddClient(const std::string& id, const json& user) {
    std::string sql = "SELECT username, added_by, added_at FROM clients WHERE id=?;";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string uname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string added_by = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        std::string added_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        sqlite3_finalize(stmt);
        return "Клиент найден.\nДобавил: " + added_by + "\nДата: " + added_at;
    }
    sqlite3_finalize(stmt);

    std::string username = user.contains("username") ? ("@" + (std::string)user["username"]) : "";
    std::string full_name = user.value("first_name", "") + " " + user.value("last_name", "");
    std::string who = !username.empty() ? username : full_name;

    time_t now = time(0);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));

    sql = "INSERT INTO clients (id, username, added_by, added_at) VALUES (?, ?, ?, ?);";
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, who.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, time_str, -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return "Клиент не найден. Добавлен в базу.";
}

std::string getAllClients() {
    std::string result;
    const char *sql = "SELECT id, added_by, added_at FROM clients;";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    while(sqlite3_step(stmt) == SQLITE_ROW) {
        std::string id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string who = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        std::string when = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        result += id + " (добавил: " + who + ", " + when + ")\n";
    }
    sqlite3_finalize(stmt);
    return result.empty() ? "База пуста." : result;
}

void handleCallback(const json& callback) {
    long chat_id = callback["message"]["chat"]["id"];
    std::string data = callback["data"];

    if (data == "check") {
        sendMessage(chat_id, "Введите ID для проверки: (например: check 123)");
    } else if (data == "add") {
        sendMessage(chat_id, "Введите ID для добавления: (например: add 123)");
    } else if (data == "list") {
        sendMessage(chat_id, getAllClients());
    }
}

void handleUpdate(const json& update) {
    if(update.contains("callback_query")) {
        handleCallback(update["callback_query"]);
        return;
    }

    if(!update.contains("message")) return;
    auto message = update["message"];
    if(!message.contains("text")) return;

    std::string text = message["text"];
    long chat_id = message["chat"]["id"];

    if(text == "/start") {
        sendMessageWithButtons(chat_id, "Привет! Выберите действие:");
    } else if (text.rfind("check ", 0) == 0) {
        std::string id = text.substr(6);
        sendMessage(chat_id, checkOrAddClient(id, message["from"]));
    } else if (text.rfind("add ", 0) == 0) {
        std::string id = text.substr(4);
        sendMessage(chat_id, checkOrAddClient(id, message["from"]));
    } else {
        sendMessage(chat_id, "Введите команду:\ncheck <id> — проверить\nadd <id> — добавить");
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
