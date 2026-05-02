#include <iostream>
#include <string>
#include <cstring>
#include <crypt.h>
#include "httplib.h"
#include "nlohmann/json.hpp"
#include <mysql/mysql.h>
#include <cstdlib>
#include <stdexcept>
#include <ctime>
#include <chrono>
#include <random>
#include <vector>
#include <unordered_map>
#define JWT_DISABLE_PICOJSON
#include "external/jwt-cpp/include/jwt-cpp/jwt.h"
#include "external/jwt-cpp/include/jwt-cpp/traits/nlohmann-json/traits.h"


using json_traits = jwt::traits::nlohmann_json;

using namespace std;
using json = nlohmann::json;

struct UserAuthRequest
{
    string username;
    string password;
};

struct ApiResult
{
    bool success = false;
    string result;
    string message;
    int user_id = 0;

    // token 필드
    string username;
    string token;
};

struct DBConfig
{
    string host;
    string user;
    string password;
    string db_name;
    unsigned int port = 3306;
};

// JWT 설정 구조체
struct JWTConfig
{
    string secret;
    string issuer;
};

// 채팅 메세지 구조체
struct ChatMessage
{
    int id = 0;
    int user_id = 0;
    string username;
    string message;
    string created_at;
};

// 캐릭터 스테이터스 구조체
struct CharacterData
{
    int id = 0;
    int user_id = 0;
    string name;

    int level = 1;
    int exp = 0;
    int gold = 0;

    int hp = 100;
    int max_hp = 100;
    int mp = 50;
    int max_mp = 50;

    int attack = 10;
    int defense = 5;

    float pos_x = 0.0f;
    float pos_y = 0.0f;
    float pos_z = 0.0f;

    float yaw = 0.0f;

    string created_at;
    string updated_at;
};

// 캐릭터 요청용 구조체
struct CreateCharacterRequest
{
    string name;
};

struct DeleteCharacterRequest
{
    int character_id = 0;
};

// 월드 전용 구조체
struct WorldEnterRequest
{
    int character_id = 0;
};

struct WorldMoveRequest
{
    int character_id = 0;
    float pos_x = 0.0f;
    float pos_y = 0.0f;
    float pos_z = 0.0f;

    float yaw = 0.0f;
};

struct WorldLeaveRequest
{
    int character_id = 0;
};

// 온라인 플레이어 상태(강제 접속 종료 케이스를 위해 필요)
struct OnlinePlayerState
{
    CharacterData character;
    std::chrono::steady_clock::time_point last_seen;
};

// 접속 중인 플레이어들 
unordered_map<int, OnlinePlayerState> online_players;

bool can_create_more_characters(MYSQL* conn, int user_id, int max_count, std::string& out_error);
bool get_characters_by_user_id(MYSQL* conn, int user_id, std::vector<CharacterData>& out_characters, std::string& out_error);
bool create_character(MYSQL* conn, int user_id, const std::string& character_name, CharacterData& out_character, std::string& out_error);
bool get_character_by_id(MYSQL* conn, int user_id, int character_id, CharacterData& out_character, std::string& out_error);
bool delete_character_by_id(MYSQL* conn, int user_id, int character_id, std::string& out_error);
bool parse_create_character_request(const string& body, CreateCharacterRequest& out_request, string& out_error);
bool parse_delete_character_request(const string& body, DeleteCharacterRequest& out_request, string& out_error);


// 환경 변수 읽기 헬퍼 함수
bool get_env_string(const char* key, string& out_value)
{
    const char* value = getenv(key);
    if (value == nullptr)
    {
        return false;
    }

    out_value = value;
    return !out_value.empty();
}

bool get_env_uint(const char* key, unsigned int& out_value)
{
    const char* value = getenv(key);
    if (value == nullptr)
    {
        return false;
    }

    try
    {
        unsigned long parsed = stoul(value);
        out_value = static_cast<unsigned int>(parsed);
        return true;
    }
    catch (const exception&)
    {
        return false;
    }
}

// DB 설정 로드 함수
bool load_db_config_from_env(DBConfig& out_config, string& out_error)
{
    if (!get_env_string("DB_HOST", out_config.host))
    {
        out_error = "Missing DB_HOST";
        return false;
    }

    if (!get_env_string("DB_USER", out_config.user))
    {
        out_error = "Missing DB_USER";
        return false;
    }

    if (!get_env_string("DB_PASSWORD", out_config.password))
    {
        out_error = "Missing DB_PASSWORD";
        return false;
    }

    if (!get_env_string("DB_NAME", out_config.db_name))
    {
        out_error = "Missing DB_NAME";
        return false;
    }

    if (!get_env_uint("DB_PORT", out_config.port))
    {
        out_error = "Missing or invalid DB_PORT";
        return false;
    }

    return true;
}

/////////////////////////////////////

// JWT 환경 변수 로더
bool load_jwt_config_from_env(JWTConfig& out_config, string& out_error)
{
    if (!get_env_string("JWT_SECRET", out_config.secret))
    {
        out_error = "Missing JWT_SECRET";
        return false;
    }

    if (!get_env_string("JWT_ISSUER", out_config.issuer))
    {
        out_error = "Missing JWT_ISSUER";
        return false;
    }

    return true;
}

// jti 생성 함수 추가
// RFC 7519의 jti는 JWT 고유 식별자.
string generate_jti()
{
    static const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    random_device rd;  // 운영체제에서 제공하는 하드웨어 엔트로피를 기반으로 난수(시드값)를 생성
    mt19937 gen(rd()); // 메르센 트위스터 알고리즘(Mersenne Twister, 32비트) 난수 생성기
    uniform_int_distribution<int> dist(0, sizeof(charset) - 2);
    // charset의 인덱스를 랜덤하게 뽑기 위해 사용
    // charset은 문자열 배열이라 맨 끝에 자동으로 널 문자 '\0' 가 하나 더 들어가기에 -2를 사용

    string result;
    result.reserve(32);
    // 문자열이 최소 32글자 들어갈 메모리 공간을 미리 확보
    // 반복문에서 문자 하나씩 붙이면 문자열 크기가 커질 때마다 생기는 붎필요한 메모리 재할당을 줄임

    for (int i = 0; i < 32; ++i)
    {
        // gen이 만들어낸 난수를 이용해서 0 ~ 61 범위의 정수 하나를 균등하게 뽑음
        result += charset[dist(gen)];
    }

    return result;
}

// 1시간 유효한 JWT 생성 함수 추가
string create_jwt_token(int user_id, const string& username, const JWTConfig& jwt_config)
{
    auto now = chrono::system_clock::now();
    auto expires_at = now + chrono::hours{ 1 };

    string jti = generate_jti();

    string token = jwt::create<json_traits>()
        .set_type("JWT")
        .set_issuer(jwt_config.issuer)
        .set_subject(to_string(user_id))
        .set_payload_claim("username", jwt::basic_claim<json_traits>(username))
        .set_issued_at(now)
        .set_expires_at(expires_at)
        .set_id(jti)
        .sign(jwt::algorithm::hs256{ jwt_config.secret });

    return token;
}

// Authorization 헤더 파서 추가 (HTTP 요청 헤더에서 Bearer 토큰만 뽑아내는 함수)
bool extract_bearer_token(const httplib::Request& req, string& out_token)
{
    auto auth_header = req.get_header_value("Authorization");
    const string prefix = "Bearer ";

    if (auth_header.size() <= prefix.size())
    {
        return false;
    }

    if (auth_header.compare(0, prefix.size(), prefix) != 0)
    {
        return false;
    }

    out_token = auth_header.substr(prefix.size());
    return !out_token.empty();
}

// JWT 검증 함수
bool verify_jwt_token(
    const string& token,
    const JWTConfig& jwt_config,
    int& out_user_id,
    string& out_username,
    string& out_error)
{
    try
    {
        auto decoded = jwt::decode<json_traits>(token);

        auto verifier = jwt::verify<json_traits>()
            .allow_algorithm(jwt::algorithm::hs256{ jwt_config.secret })
            .with_issuer(jwt_config.issuer);

        verifier.verify(decoded);

        if (!decoded.has_subject())
        {
            out_error = "Token missing subject";
            return false;
        }

        out_user_id = stoi(decoded.get_subject());

        auto payload = decoded.get_payload_json();
        auto it = payload.find("username");
        if (it == payload.end() || !it->second.is_string())
        {
            out_error = "Token missing username";
            return false;
        }

        out_username = it->second.get<string>();
        return true;
    }
    catch (const exception& e)
    {
        out_error = e.what();
        return false;
    }
}

/////////////////////////////////////


bool parse_user_auth_request(const string& body, UserAuthRequest& out_request, string& out_error)
{
    try
    {
        json request_json = json::parse(body);

        if (!request_json.contains("username") || !request_json.contains("password"))
        {
            out_error = "Missing username or password";
            return false;
        }

        if (!request_json["username"].is_string() || !request_json["password"].is_string())
        {
            out_error = "username/password must be string";
            return false;
        }

        out_request.username = request_json["username"].get<string>();
        out_request.password = request_json["password"].get<string>();

        if (out_request.username.empty() || out_request.password.empty())
        {
            out_error = "username/password cannot be empty";
            return false;
        }

        if (out_request.username.size() > 50)
        {
            out_error = "username too long";
            return false;
        }

        // bcrypt는 72자까지만 의미 있게 처리됨
        if (out_request.password.size() > 72)
        {
            out_error = "password too long (max 72 chars for bcrypt)";
            return false;
        }

        return true;
    }
    catch (const exception&)
    {
        out_error = "Invalid JSON";
        return false;
    }
}

MYSQL* connect_db(const DBConfig& config)
{
    MYSQL* conn = mysql_init(nullptr);
    if (conn == nullptr)
    {
        return nullptr;
    }

    if (!mysql_real_connect(
        conn,
        config.host.c_str(),
        config.user.c_str(),
        config.password.c_str(),
        config.db_name.c_str(),
        config.port,
        nullptr,
        0))
    {
        mysql_close(conn);
        return nullptr;
    }

    return conn;
}

bool hash_password_bcrypt(const string& password, string& out_hash, string& out_error)
{
    if (password.size() > 72)
    {
        out_error = "Password too long for bcrypt";
        return false;
    }

    constexpr unsigned long BCRYPT_COST = 12;
    char salt_output[128] = {};

    char* salt = crypt_gensalt_rn("$2b$", BCRYPT_COST, nullptr, 0, salt_output, sizeof(salt_output));
    if (salt == nullptr)
    {
        out_error = "bcrypt salt generation failed";
        return false;
    }

    crypt_data data{};
    data.initialized = 0;

    char* hashed = crypt_r(password.c_str(), salt, &data);
    if (hashed == nullptr)
    {
        out_error = "bcrypt hashing failed";
        return false;
    }

    out_hash = hashed;
    return true;
}

bool verify_password_bcrypt(const string& password, const string& stored_hash)
{
    if (stored_hash.empty())
    {
        return false;
    }

    crypt_data data{};
    data.initialized = 0;

    char* hashed = crypt_r(password.c_str(), stored_hash.c_str(), &data);
    if (hashed == nullptr)
    {
        return false;
    }

    return stored_hash == hashed;
}

bool check_user_exists(MYSQL* conn, const string& username, bool& out_exists)
{
    out_exists = false;

    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (stmt == nullptr)
    {
        return false;
    }

    const char* query = "SELECT id FROM users WHERE username = ?";

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0)
    {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND param_bind[1] = {};
    param_bind[0].buffer_type = MYSQL_TYPE_STRING;
    param_bind[0].buffer = (void*)username.c_str();
    param_bind[0].buffer_length = static_cast<unsigned long>(username.size());

    if (mysql_stmt_bind_param(stmt, param_bind) != 0)
    {
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0)
    {
        mysql_stmt_close(stmt);
        return false;
    }

    int user_id = 0;
    bool user_id_is_null = false;

    MYSQL_BIND result_bind[1] = {};
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &user_id;
    result_bind[0].is_null = &user_id_is_null;

    if (mysql_stmt_bind_result(stmt, result_bind) != 0)
    {
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_store_result(stmt) != 0)
    {
        mysql_stmt_close(stmt);
        return false;
    }

    int fetch_status = mysql_stmt_fetch(stmt);
    out_exists = (fetch_status == 0);

    mysql_stmt_close(stmt);
    return true;
}

ApiResult handle_register(MYSQL* conn, const UserAuthRequest& request)
{
    ApiResult api_result;

    bool user_exists = false;
    if (!check_user_exists(conn, request.username, user_exists))
    {
        api_result.result = "error";
        api_result.message = "Failed to check username";
        return api_result;
    }

    if (user_exists)
    {
        api_result.result = "fail";
        api_result.message = "Username already exists";
        return api_result;
    }

    string hashed_password;
    string hash_error;

    if (!hash_password_bcrypt(request.password, hashed_password, hash_error))
    {
        api_result.result = "error";
        api_result.message = hash_error;
        return api_result;
    }

    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (stmt == nullptr)
    {
        api_result.result = "error";
        api_result.message = "Statement init failed";
        return api_result;
    }

    const char* query = "INSERT INTO users (username, password_hash) VALUES (?, ?)";

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0)
    {
        api_result.result = "error";
        api_result.message = "Statement prepare failed";
        mysql_stmt_close(stmt);
        return api_result;
    }

    MYSQL_BIND param_bind[2] = {};
    param_bind[0].buffer_type = MYSQL_TYPE_STRING;
    param_bind[0].buffer = (void*)request.username.c_str();
    param_bind[0].buffer_length = static_cast<unsigned long>(request.username.size());

    param_bind[1].buffer_type = MYSQL_TYPE_STRING;
    param_bind[1].buffer = (void*)hashed_password.c_str();
    param_bind[1].buffer_length = static_cast<unsigned long>(hashed_password.size());

    if (mysql_stmt_bind_param(stmt, param_bind) != 0)
    {
        api_result.result = "error";
        api_result.message = "Bind param failed";
        mysql_stmt_close(stmt);
        return api_result;
    }

    if (mysql_stmt_execute(stmt) != 0)
    {
        api_result.result = "error";
        api_result.message = "Register execute failed";
        mysql_stmt_close(stmt);
        return api_result;
    }

    api_result.success = true;
    api_result.result = "success";
    api_result.message = "Register Success";
    api_result.user_id = static_cast<int>(mysql_insert_id(conn));

    mysql_stmt_close(stmt);
    return api_result;
}

ApiResult handle_login(MYSQL* conn, const UserAuthRequest& request, const JWTConfig& jwt_config)
{
    ApiResult api_result;

    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (stmt == nullptr)
    {
        api_result.result = "error";
        api_result.message = "Statement init failed";
        return api_result;
    }

    const char* query = "SELECT id, password_hash FROM users WHERE username = ?";

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0)
    {
        api_result.result = "error";
        api_result.message = "Statement prepare failed";
        mysql_stmt_close(stmt);
        return api_result;
    }

    MYSQL_BIND param_bind[1] = {};
    param_bind[0].buffer_type = MYSQL_TYPE_STRING;
    param_bind[0].buffer = (void*)request.username.c_str();
    param_bind[0].buffer_length = static_cast<unsigned long>(request.username.size());

    if (mysql_stmt_bind_param(stmt, param_bind) != 0)
    {
        api_result.result = "error";
        api_result.message = "Bind param failed";
        mysql_stmt_close(stmt);
        return api_result;
    }

    if (mysql_stmt_execute(stmt) != 0)
    {
        api_result.result = "error";
        api_result.message = "Statement execute failed";
        mysql_stmt_close(stmt);
        return api_result;
    }

    int user_id = 0;
    char password_hash[256] = {};
    unsigned long password_hash_length = 0;
    bool user_id_is_null = false;
    bool password_is_null = false;

    MYSQL_BIND result_bind[2] = {};
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &user_id;
    result_bind[0].is_null = &user_id_is_null;

    result_bind[1].buffer_type = MYSQL_TYPE_STRING;
    result_bind[1].buffer = password_hash;
    result_bind[1].buffer_length = sizeof(password_hash);
    result_bind[1].length = &password_hash_length;
    result_bind[1].is_null = &password_is_null;

    if (mysql_stmt_bind_result(stmt, result_bind) != 0)
    {
        api_result.result = "error";
        api_result.message = "Bind result failed";
        mysql_stmt_close(stmt);
        return api_result;
    }

    if (mysql_stmt_store_result(stmt) != 0)
    {
        api_result.result = "error";
        api_result.message = "Store result failed";
        mysql_stmt_close(stmt);
        return api_result;
    }

    int fetch_status = mysql_stmt_fetch(stmt);

    if (fetch_status == MYSQL_NO_DATA)
    {
        api_result.result = "fail";
        api_result.message = "Wrong ID";
        mysql_stmt_close(stmt);
        return api_result;
    }

    string db_password_hash(password_hash, password_hash_length);

    if (verify_password_bcrypt(request.password, db_password_hash))
    {
        api_result.success = true;
        api_result.result = "success";
        api_result.message = "Login Success";
        api_result.user_id = user_id;
        api_result.username = request.username;
        api_result.token = create_jwt_token(user_id, request.username, jwt_config);
    }
    else
    {
        api_result.result = "fail";
        api_result.message = "Wrong Password";
    }

    mysql_stmt_close(stmt);
    return api_result;
}

json make_json_response(const ApiResult& api_result)
{
    json response_json;
    response_json["result"] = api_result.result;
    response_json["message"] = api_result.message;

    if (api_result.success)
    {
        response_json["user_id"] = api_result.user_id;
    }

    if (!api_result.username.empty())
    {
        response_json["username"] = api_result.username;
    }

    if (!api_result.token.empty())
    {
        response_json["token"] = api_result.token;
    }

    return response_json;
}

// 채팅 관련 함수

bool parse_chat_send_request(const string& body, string& out_message, string& out_error)
{
    try
    {
        json request_json = json::parse(body);

        if (!request_json.contains("message"))
        {
            out_error = "Missing message";
            return false;
        }

        if (!request_json["message"].is_string())
        {
            out_error = "message must be string";
            return false;
        }

        out_message = request_json["message"].get<string>();

        if (out_message.empty())
        {
            out_error = "message cannot be empty";
            return false;
        }

        if (out_message.size() > 300)
        {
            out_error = "message too long (max 300 chars)";
            return false;
        }

        return true;
    }
    catch (const exception&)
    {
        out_error = "Invalid JSON";
        return false;
    }
}


bool get_latest_chat_id(MYSQL* conn, int& out_latest_chat_id) {

    out_latest_chat_id = 0;

    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (stmt == nullptr)
    {
        return false;
    }

    // chat_messages 테이블에서 가장 큰 id값을 가져오되, 값이 없으면 0을 대신 반환
    const char* query = "SELECT COALESCE(MAX(id), 0) FROM chat_messages";

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0)
    {
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0)
    {
        mysql_stmt_close(stmt);
        return false;
    }

    int latest_chat_id = 0;
    bool latest_chat_id_is_null = false;

    MYSQL_BIND result_bind[1] = {};
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &latest_chat_id;
    result_bind[0].is_null = &latest_chat_id_is_null;

    if (mysql_stmt_bind_result(stmt, result_bind) != 0)
    {
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_store_result(stmt) != 0)
    {
        mysql_stmt_close(stmt);
        return false;
    }

    int fetch_status = mysql_stmt_fetch(stmt);
    if (fetch_status != 0 && fetch_status != MYSQL_NO_DATA)
    {
        mysql_stmt_close(stmt);
        return false;
    }

    out_latest_chat_id = latest_chat_id;

    mysql_stmt_close(stmt);
    return true;
}

bool insert_chat_message(MYSQL* conn, int user_id, const string& username, const string& message)
{
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (stmt == nullptr)
    {
        return false;
    }

    const char* query = "INSERT INTO chat_messages (user_id, username, message) VALUES (?, ?, ?)";

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0)
    {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND param_bind[3] = {};

    param_bind[0].buffer_type = MYSQL_TYPE_LONG;
    param_bind[0].buffer = &user_id;

    param_bind[1].buffer_type = MYSQL_TYPE_STRING;
    param_bind[1].buffer = (void*)username.c_str();
    param_bind[1].buffer_length = static_cast<unsigned long>(username.size());

    param_bind[2].buffer_type = MYSQL_TYPE_STRING;
    param_bind[2].buffer = (void*)message.c_str();
    param_bind[2].buffer_length = static_cast<unsigned long>(message.size());

    if (mysql_stmt_bind_param(stmt, param_bind) != 0)
    {
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0)
    {
        mysql_stmt_close(stmt);
        return false;
    }

    mysql_stmt_close(stmt);
    return true;
}

bool get_chat_messages_after_id(MYSQL* conn, int after_id, vector<ChatMessage>& out_messages)
{
    out_messages.clear();

    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (stmt == nullptr)
    {
        return false;
    }

    const char* query =
        "SELECT id, user_id, username, message, "
        "DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s') "
        "FROM chat_messages "
        "WHERE id > ? "
        "ORDER BY id ASC";

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0)
    {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND param_bind[1] = {};
    param_bind[0].buffer_type = MYSQL_TYPE_LONG;
    param_bind[0].buffer = &after_id;

    if (mysql_stmt_bind_param(stmt, param_bind) != 0)
    {
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0)
    {
        mysql_stmt_close(stmt);
        return false;
    }


    int id = 0;
    int user_id = 0;

    char username[51] = {};
    unsigned long username_length = 0;

    char message[1024] = {};
    unsigned long message_length = 0;

    char created_at[20] = {};
    unsigned long created_at_length = 0;

    bool id_is_null = false;
    bool user_id_is_null = false;
    bool username_is_null = false;
    bool message_is_null = false;
    bool created_at_is_null = false;

    MYSQL_BIND result_bind[5] = {};

    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &id;
    result_bind[0].is_null = &id_is_null;

    result_bind[1].buffer_type = MYSQL_TYPE_LONG;
    result_bind[1].buffer = &user_id;
    result_bind[1].is_null = &user_id_is_null;

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = username;
    result_bind[2].buffer_length = sizeof(username);
    result_bind[2].length = &username_length;
    result_bind[2].is_null = &username_is_null;

    result_bind[3].buffer_type = MYSQL_TYPE_STRING;
    result_bind[3].buffer = message;
    result_bind[3].buffer_length = sizeof(message);
    result_bind[3].length = &message_length;
    result_bind[3].is_null = &message_is_null;

    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = created_at;
    result_bind[4].buffer_length = sizeof(created_at);
    result_bind[4].length = &created_at_length;
    result_bind[4].is_null = &created_at_is_null;

    if (mysql_stmt_bind_result(stmt, result_bind) != 0)
    {
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_store_result(stmt) != 0)
    {
        mysql_stmt_close(stmt);
        return false;
    }

    // mysql_stmt_fetch()는 한 번 호출할 때마다 "한 행(row)"만 가져오기 때문에
    // 여러 행을 전부 가져오려면 반복문이 필요
    while (true)
    {
        int fetch_status = mysql_stmt_fetch(stmt);

        // 더 이상 데이터 없음 → 반복 종료
        if (fetch_status == MYSQL_NO_DATA)
        {
            break;
        }
        // 버퍼 부족 (데이터 잘림)
        if (fetch_status == 1 || fetch_status == MYSQL_DATA_TRUNCATED)
        {
            mysql_stmt_close(stmt);
            return false;
        }

        ChatMessage chat_message;
        chat_message.id = id;
        chat_message.user_id = user_id;
        chat_message.username = string(username, username_length);
        chat_message.message = string(message, message_length);
        chat_message.created_at = string(created_at, created_at_length);

        out_messages.push_back(chat_message);
    }

    mysql_stmt_close(stmt);
    return true;

}

bool parse_after_id(const httplib::Request& req, int& out_after_id, string& out_error)
{
    out_after_id = 0;

    if (!req.has_param("after_id"))
    {
        out_error = "Missing after_id";
        return false;
    }

    string after_id_str = req.get_param_value("after_id");

    try
    {
        int parsed = stoi(after_id_str);

        if (parsed < 0)
        {
            out_error = "after_id must be >= 0";
            return false;
        }

        out_after_id = parsed;
        return true;
    }
    catch (const exception&)
    {
        out_error = "Invalid after_id";
        return false;
    }
}
/////////////////////////////////////////////////////

// 캐릭터 관련 함수
bool can_create_more_characters(MYSQL* conn, int user_id, int max_count, std::string& out_error)
{
    if (conn == nullptr)
    {
        out_error = "MySQL connection is null";
        return false;
    }

    if (max_count <= 0)
    {
        out_error = "max_count must be greater than 0";
        return false;
    }

    const char* query =
        "SELECT COUNT(*) "
        "FROM characters "
        "WHERE user_id = ?";

    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (stmt == nullptr)
    {
        out_error = "mysql_stmt_init failed";
        return false;
    }

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0)
    {
        out_error = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND param_bind[1]{};
    param_bind[0].buffer_type = MYSQL_TYPE_LONG;
    param_bind[0].buffer = &user_id;

    if (mysql_stmt_bind_param(stmt, param_bind) != 0)
    {
        out_error = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0)
    {
        out_error = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        return false;
    }

    int character_count = 0;
    bool is_null = false;
    bool error = false;

    MYSQL_BIND result_bind[1]{};
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &character_count;
    result_bind[0].is_null = &is_null;
    result_bind[0].error = &error;

    if (mysql_stmt_bind_result(stmt, result_bind) != 0)
    {
        out_error = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_store_result(stmt) != 0)
    {
        out_error = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        return false;
    }

    int fetch_result = mysql_stmt_fetch(stmt);
    if (fetch_result == 1 || fetch_result == MYSQL_NO_DATA)
    {
        out_error = "Failed to fetch character count";
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        return false;
    }

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);

    if (character_count >= max_count)
    {
        out_error = "Character slot limit reached";
        return false;
    }

    return true;
}


bool get_characters_by_user_id(MYSQL* conn, int user_id, std::vector<CharacterData>& out_characters, std::string& out_error)
{
    constexpr int MAX_CHARACTER_COUNT = 3;

    if (!can_create_more_characters(conn, user_id, MAX_CHARACTER_COUNT, out_error))
    {
        return false;
    }

    out_characters.clear();

    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (stmt == nullptr)
    {
        out_error = "mysql_stmt_init failed";
        return false;
    }

    const char* query =
        "SELECT id, user_id, name, level, exp, gold, hp, max_hp, mp, max_mp, attack, defense, pos_x, pos_y, pos_z, yaw,"
        "DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s'), "
        "DATE_FORMAT(updated_at, '%Y-%m-%d %H:%i:%s') "
        "FROM characters "
        "WHERE user_id = ? "
        "ORDER BY id ASC";

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0)
    {
        out_error = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND param_bind[1]{};
    param_bind[0].buffer_type = MYSQL_TYPE_LONG;
    param_bind[0].buffer = &user_id;

    if (mysql_stmt_bind_param(stmt, param_bind) != 0)
    {
        out_error = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0)
    {
        out_error = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        return false;
    }

    int id = 0;
    int db_user_id = 0;
    char name[51]{};
    unsigned long name_length = 0;

    int level = 0;
    int exp = 0;
    int gold = 0;
    int hp = 0;
    int max_hp = 0;
    int mp = 0;
    int max_mp = 0;
    int attack = 0;
    int defense = 0;

    float pos_x = 0.0f;
    float pos_y = 0.0f;
    float pos_z = 0.0f;

    float yaw = 0.0f;

    char created_at[20]{};
    unsigned long created_at_length = 0;
    char updated_at[20]{};
    unsigned long updated_at_length = 0;

    bool is_null[17]{};
    bool error[17]{};

    MYSQL_BIND result_bind[18]{};

    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &id;
    result_bind[0].is_null = &is_null[0];
    result_bind[0].error = &error[0];

    result_bind[1].buffer_type = MYSQL_TYPE_LONG;
    result_bind[1].buffer = &db_user_id;
    result_bind[1].is_null = &is_null[1];
    result_bind[1].error = &error[1];

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = name;
    result_bind[2].buffer_length = sizeof(name);
    result_bind[2].length = &name_length;
    result_bind[2].is_null = &is_null[2];
    result_bind[2].error = &error[2];

    result_bind[3].buffer_type = MYSQL_TYPE_LONG;
    result_bind[3].buffer = &level;

    result_bind[4].buffer_type = MYSQL_TYPE_LONG;
    result_bind[4].buffer = &exp;

    result_bind[5].buffer_type = MYSQL_TYPE_LONG;
    result_bind[5].buffer = &gold;

    result_bind[6].buffer_type = MYSQL_TYPE_LONG;
    result_bind[6].buffer = &hp;

    result_bind[7].buffer_type = MYSQL_TYPE_LONG;
    result_bind[7].buffer = &max_hp;

    result_bind[8].buffer_type = MYSQL_TYPE_LONG;
    result_bind[8].buffer = &mp;

    result_bind[9].buffer_type = MYSQL_TYPE_LONG;
    result_bind[9].buffer = &max_mp;

    result_bind[10].buffer_type = MYSQL_TYPE_LONG;
    result_bind[10].buffer = &attack;

    result_bind[11].buffer_type = MYSQL_TYPE_LONG;
    result_bind[11].buffer = &defense;

    result_bind[12].buffer_type = MYSQL_TYPE_FLOAT;
    result_bind[12].buffer = &pos_x;

    result_bind[13].buffer_type = MYSQL_TYPE_FLOAT;
    result_bind[13].buffer = &pos_y;

    result_bind[14].buffer_type = MYSQL_TYPE_FLOAT;
    result_bind[14].buffer = &pos_z;

    result_bind[15].buffer_type = MYSQL_TYPE_FLOAT;
    result_bind[15].buffer = &yaw;

    result_bind[16].buffer_type = MYSQL_TYPE_STRING;
    result_bind[16].buffer = created_at;
    result_bind[16].buffer_length = sizeof(created_at);
    result_bind[16].length = &created_at_length;

    result_bind[17].buffer_type = MYSQL_TYPE_STRING;
    result_bind[17].buffer = updated_at;
    result_bind[17].buffer_length = sizeof(updated_at);
    result_bind[17].length = &updated_at_length;

    if (mysql_stmt_bind_result(stmt, result_bind) != 0)
    {
        out_error = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_store_result(stmt) != 0)
    {
        out_error = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        return false;
    }

    while (true)
    {
        int fetch_result = mysql_stmt_fetch(stmt);

        if (fetch_result == MYSQL_NO_DATA)
        {
            break;
        }

        if (fetch_result == 1)
        {
            out_error = mysql_stmt_error(stmt);
            mysql_stmt_close(stmt);
            return false;
        }

        CharacterData character;
        character.id = id;
        character.user_id = db_user_id;
        character.name.assign(name, name_length);
        character.level = level;
        character.exp = exp;
        character.gold = gold;
        character.hp = hp;
        character.max_hp = max_hp;
        character.mp = mp;
        character.max_mp = max_mp;
        character.attack = attack;
        character.defense = defense;
        character.pos_x = pos_x;
        character.pos_y = pos_y;
        character.pos_z = pos_z;
        character.yaw = yaw;
        character.created_at.assign(created_at, created_at_length);
        character.updated_at.assign(updated_at, updated_at_length);

        out_characters.push_back(character);
    }

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return true;
}
bool create_character(MYSQL* conn, int user_id, const std::string& character_name, CharacterData& out_character, std::string& out_error)
{
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (stmt == nullptr)
    {
        out_error = "mysql_stmt_init failed";
        return false;
    }

    const char* query =
        "INSERT INTO characters "
        "(user_id, name, level, exp, gold, hp, max_hp, mp, max_mp, attack, defense, pos_x, pos_y, pos_z, yaw) "
        "VALUES (?, ?, 1, 0, 100, 100, 100, 50, 50, 10, 5, 0, 0, 0)";

    if (mysql_stmt_prepare(stmt, query, strlen(query)) != 0)
    {
        out_error = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND param_bind[2]{};

    param_bind[0].buffer_type = MYSQL_TYPE_LONG;
    param_bind[0].buffer = &user_id;

    unsigned long name_length = static_cast<unsigned long>(character_name.size());
    param_bind[1].buffer_type = MYSQL_TYPE_STRING;
    param_bind[1].buffer = const_cast<char*>(character_name.c_str());
    param_bind[1].buffer_length = name_length;
    param_bind[1].length = &name_length;

    if (mysql_stmt_bind_param(stmt, param_bind) != 0)
    {
        out_error = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0)
    {
        out_error = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        return false;
    }

    int new_character_id = static_cast<int>(mysql_insert_id(conn));

    mysql_stmt_close(stmt);

    return get_character_by_id(conn, user_id, new_character_id, out_character, out_error);

}
bool get_character_by_id(MYSQL* conn, int user_id, int character_id, CharacterData& out_character, std::string& out_error)
{
    const char* query =
        "SELECT id, user_id, name, level, exp, gold, hp, max_hp, mp, max_mp, attack, defense, pos_x, pos_y, pos_z, yaw,"
        "DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s'), "
        "DATE_FORMAT(updated_at, '%Y-%m-%d %H:%i:%s') "
        "FROM characters "
        "WHERE id = ? AND user_id = ? "
        "LIMIT 1";

    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (stmt == nullptr)
    {
        out_error = "mysql_stmt_init failed";
        return false;
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query)) != 0)
    {
        out_error = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND param_bind[2]{};
    param_bind[0].buffer_type = MYSQL_TYPE_LONG;
    param_bind[0].buffer = &character_id;

    param_bind[1].buffer_type = MYSQL_TYPE_LONG;
    param_bind[1].buffer = &user_id;

    if (mysql_stmt_bind_param(stmt, param_bind) != 0)
    {
        out_error = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0)
    {
        out_error = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        return false;
    }

    int id = 0;
    int db_user_id = 0;
    char name[51]{};
    unsigned long name_length = 0;

    int level = 0;
    int exp = 0;
    int gold = 0;
    int hp = 0;
    int max_hp = 0;
    int mp = 0;
    int max_mp = 0;
    int attack = 0;
    int defense = 0;

    float pos_x = 0.0f;
    float pos_y = 0.0f;
    float pos_z = 0.0f;

    float yaw = 0.0f;

    char created_at[20]{};
    unsigned long created_at_length = 0;
    char updated_at[20]{};
    unsigned long updated_at_length = 0;

    MYSQL_BIND result_bind[18]{};

    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &id;

    result_bind[1].buffer_type = MYSQL_TYPE_LONG;
    result_bind[1].buffer = &db_user_id;

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = name;
    result_bind[2].buffer_length = sizeof(name);
    result_bind[2].length = &name_length;

    result_bind[3].buffer_type = MYSQL_TYPE_LONG;
    result_bind[3].buffer = &level;

    result_bind[4].buffer_type = MYSQL_TYPE_LONG;
    result_bind[4].buffer = &exp;

    result_bind[5].buffer_type = MYSQL_TYPE_LONG;
    result_bind[5].buffer = &gold;

    result_bind[6].buffer_type = MYSQL_TYPE_LONG;
    result_bind[6].buffer = &hp;

    result_bind[7].buffer_type = MYSQL_TYPE_LONG;
    result_bind[7].buffer = &max_hp;

    result_bind[8].buffer_type = MYSQL_TYPE_LONG;
    result_bind[8].buffer = &mp;

    result_bind[9].buffer_type = MYSQL_TYPE_LONG;
    result_bind[9].buffer = &max_mp;

    result_bind[10].buffer_type = MYSQL_TYPE_LONG;
    result_bind[10].buffer = &attack;

    result_bind[11].buffer_type = MYSQL_TYPE_LONG;
    result_bind[11].buffer = &defense;

    result_bind[12].buffer_type = MYSQL_TYPE_FLOAT;
    result_bind[12].buffer = &pos_x;

    result_bind[13].buffer_type = MYSQL_TYPE_FLOAT;
    result_bind[13].buffer = &pos_y;

    result_bind[14].buffer_type = MYSQL_TYPE_FLOAT;
    result_bind[14].buffer = &pos_z;

    result_bind[15].buffer_type = MYSQL_TYPE_FLOAT;
    result_bind[15].buffer = &pos_z;

    result_bind[16].buffer_type = MYSQL_TYPE_STRING;
    result_bind[16].buffer = created_at;
    result_bind[16].buffer_length = sizeof(created_at);
    result_bind[16].length = &created_at_length;

    result_bind[17].buffer_type = MYSQL_TYPE_STRING;
    result_bind[17].buffer = updated_at;
    result_bind[17].buffer_length = sizeof(updated_at);
    result_bind[17].length = &updated_at_length;

    if (mysql_stmt_bind_result(stmt, result_bind) != 0)
    {
        out_error = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_store_result(stmt) != 0)
    {
        out_error = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        return false;
    }

    int row_count = static_cast<int>(mysql_stmt_num_rows(stmt));
    if (row_count <= 0)
    {
        out_error = "Character not found";
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        return false;
    }

    int fetch_result = mysql_stmt_fetch(stmt);
    if (fetch_result == 1 || fetch_result == MYSQL_NO_DATA)
    {
        out_error = "Character fetch failed";
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        return false;
    }

    out_character.id = id;
    out_character.user_id = db_user_id;
    out_character.name.assign(name, name_length);
    out_character.level = level;
    out_character.exp = exp;
    out_character.gold = gold;
    out_character.hp = hp;
    out_character.max_hp = max_hp;
    out_character.mp = mp;
    out_character.max_mp = max_mp;
    out_character.attack = attack;
    out_character.defense = defense;
    out_character.pos_x = pos_x;
    out_character.pos_y = pos_y;
    out_character.pos_z = pos_z;
    out_character.yaw = yaw;
    out_character.created_at.assign(created_at, created_at_length);
    out_character.updated_at.assign(updated_at, updated_at_length);

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return true;
}
bool delete_character_by_id(MYSQL* conn, int user_id, int character_id, std::string& out_error)
{
    const char* query =
        "DELETE FROM characters "
        "WHERE id = ? AND user_id = ?";

    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (stmt == nullptr)
    {
        out_error = "mysql_stmt_init failed";
        return false;
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query)) != 0)
    {
        out_error = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND param_bind[2]{};

    param_bind[0].buffer_type = MYSQL_TYPE_LONG;
    param_bind[0].buffer = &character_id;

    param_bind[1].buffer_type = MYSQL_TYPE_LONG;
    param_bind[1].buffer = &user_id;

    if (mysql_stmt_bind_param(stmt, param_bind) != 0)
    {
        out_error = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0)
    {
        out_error = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        return false;
    }

    my_ulonglong affected_rows = mysql_stmt_affected_rows(stmt);
    mysql_stmt_close(stmt);

    if (affected_rows == 0)
    {
        out_error = "Character not found or not owned by user";
        return false;
    }

    return true;
}

bool parse_create_character_request(const string& body, CreateCharacterRequest& out_request, string& out_error)
{
    try
    {
        json request_json = json::parse(body);

        if (!request_json.contains("name"))
        {
            out_error = "Missing character name";
            return false;
        }

        if (!request_json["name"].is_string())
        {
            out_error = "Character name must be string";
            return false;
        }

        out_request.name = request_json["name"].get<string>();

        if (out_request.name.empty())
        {
            out_error = "Character name cannot be empty";
            return false;
        }

        if (out_request.name.size() > 50)
        {
            out_error = "Character name too long";
            return false;
        }

        return true;
    }
    catch (const exception& e)
    {
        out_error = e.what();
        return false;
    }
}

bool parse_delete_character_request(const string& body, DeleteCharacterRequest& out_request, string& out_error)
{
    try
    {
        json request_json = json::parse(body);

        if (!request_json.contains("character_id"))
        {
            out_error = "Missing character_id";
            return false;
        }

        if (!request_json["character_id"].is_number_integer())
        {
            out_error = "character_id must be integer";
            return false;
        }

        out_request.character_id = request_json["character_id"].get<int>();

        if (out_request.character_id <= 0)
        {
            out_error = "Invalid character_id";
            return false;
        }

        return true;
    }
    catch (const exception& e)
    {
        out_error = e.what();
        return false;
    }
}

/////////////////////////////////////////////////////
// 월드 관련 함수들
// 월드 입장
bool parse_world_enter_request(const string& body, WorldEnterRequest& out_request, string& out_error)
{
    try
    {
        json request_json = json::parse(body);

        if (!request_json.contains("character_id"))
        {
            out_error = "Missing character_id";
            return false;
        }

        if (!request_json["character_id"].is_number_integer())
        {
            out_error = "character_id must be integer";
            return false;
        }

        out_request.character_id = request_json["character_id"].get<int>();

        if (out_request.character_id <= 0)
        {
            out_error = "Invalid character_id";
            return false;
        }

        return true;
    }
    catch (const exception& e)
    {
        out_error = e.what();
        return false;
    }
}
// 월드 움직임 요청
bool parse_world_move_request(const string& body, WorldMoveRequest& out_request, string& out_error)
{
    try
    {
        json request_json = json::parse(body);

        if (!request_json.contains("character_id") ||
            !request_json.contains("pos_x") ||
            !request_json.contains("pos_y") ||
            !request_json.contains("pos_z") ||
            !request_json.contains("yaw"))
        {
            out_error = "Missing required fields";
            return false;
        }

        if (!request_json["character_id"].is_number_integer())
        {
            out_error = "character_id must be integer";
            return false;
        }

        if (!request_json["pos_x"].is_number() ||
            !request_json["pos_y"].is_number() ||
            !request_json["pos_z"].is_number() ||
            !request_json["yaw"].is_number())
        {
            out_error = "pos_x, pos_y, pos_z, yaw must be numbers";
            return false;
        }

        out_request.character_id = request_json["character_id"].get<int>();
        out_request.pos_x = request_json["pos_x"].get<float>();
        out_request.pos_y = request_json["pos_y"].get<float>();
        out_request.pos_z = request_json["pos_z"].get<float>();
        out_request.yaw = request_json["yaw"].get<float>();

        if (out_request.character_id <= 0)
        {
            out_error = "Invalid character_id";
            return false;
        }

        return true;
    }
    catch (const exception& e)
    {
        out_error = e.what();
        return false;
    }
}
// 월드 퇴장 요청
bool parse_world_leave_request(const string& body, WorldLeaveRequest& out_request, string& out_error)
{
    try
    {
        json request_json = json::parse(body);

        if (!request_json.contains("character_id"))
        {
            out_error = "Missing character_id";
            return false;
        }

        if (!request_json["character_id"].is_number_integer())
        {
            out_error = "character_id must be integer";
            return false;
        }

        out_request.character_id = request_json["character_id"].get<int>();

        if (out_request.character_id <= 0)
        {
            out_error = "Invalid character_id";
            return false;
        }

        return true;
    }
    catch (const exception& e)
    {
        out_error = e.what();
        return false;
    }
}
// 월드 퇴장 요청(강제 종료시)
void remove_timed_out_players()
{
    using namespace std::chrono;

    constexpr int TIMEOUT_SECONDS = 15;
    auto now = steady_clock::now();

    vector<int> to_remove;

    for (const auto& [character_id, state] : online_players)
    {
        auto elapsed = duration_cast<seconds>(now - state.last_seen).count();

        if (elapsed >= TIMEOUT_SECONDS)
        {
            to_remove.push_back(character_id);
        }
    }

    for (int character_id : to_remove)
    {
        online_players.erase(character_id);
    }
}
// 월드에 플레이어가 살아있는지 검사(플레이어의 last seen 업데이트)
void update_last_seen_by_character_id(int character_id)
{
    auto it = online_players.find(character_id);
    if (it != online_players.end())
    {
        it->second.last_seen = chrono::steady_clock::now();
    }
}
// user id로 온라인 유저 찾기
bool find_online_character_by_user_id(int user_id, int& out_character_id)
{
    for (const auto& [character_id, state] : online_players)
    {
        if (state.character.user_id == user_id)
        {
            out_character_id = character_id;
            return true;
        }
    }

    return false;
}

/////////////////////////////////////////////////////
int main()
{
    DBConfig db_config;
    string config_error;

    if (!load_db_config_from_env(db_config, config_error))
    {
        cerr << "Failed to load DB config: " << config_error << endl;
        return 1;
    }

    JWTConfig jwt_config;
    string jwt_error;

    if (!load_jwt_config_from_env(jwt_config, jwt_error))
    {
        cerr << "Failed to load JWT config: " << jwt_error << endl;
        return 1;
    }

    httplib::Server server;

    server.Get("/", [](const httplib::Request& req, httplib::Response& res) {
        res.status = 200;
        res.set_content("Game Server Running", "text/plain");
        });

    server.Post("/register", [&db_config](const httplib::Request& req, httplib::Response& res) {

        cout << "[REGISTER] Request Body: " << req.body << endl;

        UserAuthRequest auth_request;
        string parse_error;

        if (!parse_user_auth_request(req.body, auth_request, parse_error))
        {
            json error_json;
            error_json["result"] = "error";
            error_json["message"] = parse_error;

            res.status = 400;
            res.set_content(error_json.dump(), "application/json");
            return;
        }

        MYSQL* conn = connect_db(db_config);
        if (conn == nullptr)
        {
            json error_json;
            error_json["result"] = "error";
            error_json["message"] = "DB connection failed";

            res.status = 500;
            res.set_content(error_json.dump(), "application/json");
            return;
        }

        ApiResult register_result = handle_register(conn, auth_request);
        json response_json = make_json_response(register_result);

        if (register_result.success)
        {
            res.status = 201;
        }
        else if (register_result.result == "fail")
        {
            res.status = 409;
        }
        else
        {
            res.status = 500;
        }

        res.set_content(response_json.dump(), "application/json");
        mysql_close(conn);
        });

    server.Post("/login", [&db_config, &jwt_config](const httplib::Request& req, httplib::Response& res) {

        cout << "[LOGIN] Request Body: " << req.body << endl;

        UserAuthRequest auth_request;
        string parse_error;

        if (!parse_user_auth_request(req.body, auth_request, parse_error))
        {
            json error_json;
            error_json["result"] = "error";
            error_json["message"] = parse_error;

            res.status = 400;
            res.set_content(error_json.dump(), "application/json");
            return;
        }

        MYSQL* conn = connect_db(db_config);
        if (conn == nullptr)
        {
            json error_json;
            error_json["result"] = "error";
            error_json["message"] = "DB connection failed";

            res.status = 500;
            res.set_content(error_json.dump(), "application/json");
            return;
        }

        ApiResult login_result = handle_login(conn, auth_request, jwt_config);
        json response_json = make_json_response(login_result);

        if (login_result.success)
        {
            res.status = 200;
        }
        else if (login_result.result == "fail")
        {
            res.status = 401;
        }
        else
        {
            res.status = 500;
        }

        res.set_content(response_json.dump(), "application/json");
        mysql_close(conn);
        });

    server.Get("/me", [&jwt_config](const httplib::Request& req, httplib::Response& res) {

        string token;
        if (!extract_bearer_token(req, token))
        {
            json error_json;
            error_json["result"] = "fail";
            error_json["message"] = "Missing or invalid Authorization header";

            res.status = 401;
            res.set_content(error_json.dump(), "application/json");
            return;
        }

        int user_id = 0;
        string username;
        string verify_error;

        if (!verify_jwt_token(token, jwt_config, user_id, username, verify_error))
        {
            json error_json;
            error_json["result"] = "fail";
            error_json["message"] = "Invalid token";
            error_json["detail"] = verify_error;

            res.status = 401;
            res.set_content(error_json.dump(), "application/json");
            return;
        }

        json response_json;
        response_json["result"] = "success";
        response_json["message"] = "Token Valid";
        response_json["user_id"] = user_id;
        response_json["username"] = username;

        res.status = 200;
        res.set_content(response_json.dump(), "application/json");
        });


    // 채팅
    // 채팅방 입장
    server.Get("/chat/enter", [&db_config, &jwt_config](const httplib::Request& req, httplib::Response& res) {

        string token;
        if (!extract_bearer_token(req, token))
        {
            json error_json;
            error_json["result"] = "fail";
            error_json["message"] = "Missing or invalid Authorization header";

            res.status = 401;
            res.set_content(error_json.dump(), "application/json");
            return;
        }

        int user_id = 0;
        string username;
        string verify_error;

        if (!verify_jwt_token(token, jwt_config, user_id, username, verify_error))
        {
            json error_json;
            error_json["result"] = "fail";
            error_json["message"] = "Invalid token";
            error_json["detail"] = verify_error;

            res.status = 401;
            res.set_content(error_json.dump(), "application/json");
            return;
        }

        MYSQL* conn = connect_db(db_config);
        if (conn == nullptr)
        {
            json error_json;
            error_json["result"] = "error";
            error_json["message"] = "DB connection failed";

            res.status = 500;
            res.set_content(error_json.dump(), "application/json");
            return;
        }

        int latest_chat_id = 0;
        if (!get_latest_chat_id(conn, latest_chat_id))
        {
            json error_json;
            error_json["result"] = "error";
            error_json["message"] = "Failed to get latest chat id";

            res.status = 500;
            res.set_content(error_json.dump(), "application/json");
            mysql_close(conn);
            return;
        }

        json response_json;
        response_json["result"] = "success";
        response_json["message"] = "Chat enter success";
        response_json["latest_chat_id"] = latest_chat_id;

        res.status = 200;
        res.set_content(response_json.dump(), "application/json");
        mysql_close(conn);
        });
    // 새 메시지 폴링
    server.Get("/chat/messages", [&db_config, &jwt_config](const httplib::Request& req, httplib::Response& res) {
        string token;
        if (!extract_bearer_token(req, token))
        {
            json error_json;
            error_json["result"] = "fail";
            error_json["message"] = "Missing or invalid Authorization header";

            res.status = 401;
            res.set_content(error_json.dump(), "application/json");
            return;
        }

        int user_id = 0;
        string username;
        string verify_error;

        if (!verify_jwt_token(token, jwt_config, user_id, username, verify_error))
        {
            json error_json;
            error_json["result"] = "fail";
            error_json["message"] = "Invalid token";
            error_json["detail"] = verify_error;

            res.status = 401;
            res.set_content(error_json.dump(), "application/json");
            return;
        }

        int after_id = 0;
        string parse_error;

        if (!parse_after_id(req, after_id, parse_error))
        {
            json error_json;
            error_json["result"] = "error";
            error_json["message"] = parse_error;

            res.status = 400;
            res.set_content(error_json.dump(), "application/json");
            return;
        }

        MYSQL* conn = connect_db(db_config);
        if (conn == nullptr)
        {
            json error_json;
            error_json["result"] = "error";
            error_json["message"] = "DB connection failed";

            res.status = 500;
            res.set_content(error_json.dump(), "application/json");
            return;
        }

        vector<ChatMessage> messages;
        if (!get_chat_messages_after_id(conn, after_id, messages))
        {
            json error_json;
            error_json["result"] = "error";
            error_json["message"] = "Failed to get chat messages";

            res.status = 500;
            res.set_content(error_json.dump(), "application/json");
            mysql_close(conn);
            return;
        }

        json response_json;
        response_json["result"] = "success";
        response_json["message"] = "Chat messages fetched";
        response_json["messages"] = json::array();

        for (const auto& msg : messages)
        {
            json msg_json;
            msg_json["id"] = msg.id;
            msg_json["user_id"] = msg.user_id;
            msg_json["username"] = msg.username;
            msg_json["message"] = msg.message;
            msg_json["created_at"] = msg.created_at;

            response_json["messages"].push_back(msg_json);
        }

        res.status = 200;
        res.set_content(response_json.dump(), "application/json");
        mysql_close(conn);
        });
    // 채팅 전송
    server.Post("/chat/send", [&db_config, &jwt_config](const httplib::Request& req, httplib::Response& res) {
        string token;
        if (!extract_bearer_token(req, token))
        {
            json error_json;
            error_json["result"] = "fail";
            error_json["message"] = "Missing or invalid Authorization header";

            res.status = 401;
            res.set_content(error_json.dump(), "application/json");
            return;
        }

        int user_id = 0;
        string username;
        string verify_error;

        if (!verify_jwt_token(token, jwt_config, user_id, username, verify_error))
        {
            json error_json;
            error_json["result"] = "fail";
            error_json["message"] = "Invalid token";
            error_json["detail"] = verify_error;

            res.status = 401;
            res.set_content(error_json.dump(), "application/json");
            return;
        }

        string message_text;
        string parse_error;

        if (!parse_chat_send_request(req.body, message_text, parse_error))
        {
            json error_json;
            error_json["result"] = "error";
            error_json["message"] = parse_error;

            res.status = 400;
            res.set_content(error_json.dump(), "application/json");
            return;
        }

        MYSQL* conn = connect_db(db_config);
        if (conn == nullptr)
        {
            json error_json;
            error_json["result"] = "error";
            error_json["message"] = "DB connection failed";

            res.status = 500;
            res.set_content(error_json.dump(), "application/json");
            return;
        }

        if (!insert_chat_message(conn, user_id, username, message_text))
        {
            json error_json;
            error_json["result"] = "error";
            error_json["message"] = "Failed to send chat";

            res.status = 500;
            res.set_content(error_json.dump(), "application/json");
            mysql_close(conn);
            return;
        }

        json response_json;
        response_json["result"] = "success";
        response_json["message"] = "Chat sent";

        res.status = 201;
        res.set_content(response_json.dump(), "application/json");
        mysql_close(conn);

        });

    // 캐릭터
    // 내 캐릭터 목록 조회
    server.Get("/characters", [&db_config, &jwt_config](const httplib::Request& req, httplib::Response& res) {

        json response_json;

        string token;
        if (!extract_bearer_token(req, token))
        {
            json error_json;
            error_json["result"] = "fail";
            error_json["message"] = "Missing or invalid Authorization header";

            res.status = 401;
            res.set_content(error_json.dump(), "application/json");
            return;
        }

        int user_id = 0;
        string username;
        string verify_error;

        if (!verify_jwt_token(token, jwt_config, user_id, username, verify_error))
        {
            json error_json;
            error_json["result"] = "fail";
            error_json["message"] = "Invalid token";
            error_json["detail"] = verify_error;

            res.status = 401;
            res.set_content(error_json.dump(), "application/json");
            return;
        }

        MYSQL* conn = connect_db(db_config);
        if (conn == nullptr)
        {
            json error_json;
            error_json["result"] = "error";
            error_json["message"] = "DB connection failed";

            res.status = 500;
            res.set_content(error_json.dump(), "application/json");
            return;
        }

        vector<CharacterData> characters;
        string query_error;

        if (!get_characters_by_user_id(conn, user_id, characters, query_error))
        {
            mysql_close(conn);

            res.status = 500;
            response_json["result"] = "fail";
            response_json["message"] = "Failed to get character list: " + query_error;
            res.set_content(response_json.dump(), "application/json");
            return;
        }

        mysql_close(conn);

        response_json["result"] = "success";
        response_json["message"] = "Character list retrieved";
        response_json["user_id"] = user_id;
        response_json["username"] = username;
        response_json["characters"] = json::array();

        for (const auto& character : characters)
        {
            response_json["characters"].push_back({
                {"id", character.id},
                {"user_id", character.user_id},
                {"name", character.name},
                {"level", character.level},
                {"exp", character.exp},
                {"gold", character.gold},
                {"hp", character.hp},
                {"max_hp", character.max_hp},
                {"mp", character.mp},
                {"max_mp", character.max_mp},
                {"attack", character.attack},
                {"defense", character.defense},
                {"pos_x", character.pos_x},
                {"pos_y", character.pos_y},
                {"pos_z", character.pos_z},
                {"yaw", character.yaw},
                {"created_at", character.created_at},
                {"updated_at", character.updated_at}
                });
        }

        res.status = 200;
        res.set_content(response_json.dump(), "application/json");

        });
    // 새 캐릭터 생성
    server.Post("/characters/create", [&db_config, &jwt_config](const httplib::Request& req, httplib::Response& res) {

        json response_json;

        string token;
        if (!extract_bearer_token(req, token))
        {
            json error_json;
            error_json["result"] = "fail";
            error_json["message"] = "Missing or invalid Authorization header";

            res.status = 401;
            res.set_content(error_json.dump(), "application/json");
            return;
        }

        int user_id = 0;
        string username;
        string verify_error;

        if (!verify_jwt_token(token, jwt_config, user_id, username, verify_error))
        {
            json error_json;
            error_json["result"] = "fail";
            error_json["message"] = "Invalid token";
            error_json["detail"] = verify_error;

            res.status = 401;
            res.set_content(error_json.dump(), "application/json");
            return;
        }

        CreateCharacterRequest create_request;
        string parse_error;
        if (!parse_create_character_request(req.body, create_request, parse_error))
        {
            res.status = 400;
            response_json["result"] = "fail";
            response_json["message"] = "Invalid request body: " + parse_error;
            res.set_content(response_json.dump(), "application/json");
            return;
        }

        MYSQL* conn = connect_db(db_config);
        if (conn == nullptr)
        {
            json error_json;
            error_json["result"] = "error";
            error_json["message"] = "DB connection failed";

            res.status = 500;
            res.set_content(error_json.dump(), "application/json");
            return;
        }

        constexpr int MAX_CHARACTER_COUNT = 3;
        string limit_error;
        if (!can_create_more_characters(conn, user_id, MAX_CHARACTER_COUNT, limit_error))
        {
            mysql_close(conn);

            res.status = 400;
            response_json["result"] = "fail";
            response_json["message"] = limit_error;
            res.set_content(response_json.dump(), "application/json");
            return;
        }

        CharacterData created_character;
        string create_error;
        if (!create_character(conn, user_id, create_request.name, created_character, create_error))
        {
            mysql_close(conn);

            res.status = 400;
            response_json["result"] = "fail";
            response_json["message"] = "Character creation failed: " + create_error;
            res.set_content(response_json.dump(), "application/json");
            return;
        }

        mysql_close(conn);

        response_json["result"] = "success";
        response_json["message"] = "Character created";
        response_json["character"] = {
            {"id", created_character.id},
            {"user_id", created_character.user_id},
            {"name", created_character.name},
            {"level", created_character.level},
            {"exp", created_character.exp},
            {"gold", created_character.gold},
            {"hp", created_character.hp},
            {"max_hp", created_character.max_hp},
            {"mp", created_character.mp},
            {"max_mp", created_character.max_mp},
            {"attack", created_character.attack},
            {"defense", created_character.defense},
            {"pos_x", created_character.pos_x},
            {"pos_y", created_character.pos_y},
            {"pos_z", created_character.pos_z},
            {"yaw", created_character.yaw},
            {"created_at", created_character.created_at},
            {"updated_at", created_character.updated_at}
        };

        res.status = 200;
        res.set_content(response_json.dump(), "application/json");

        });
    // 선택한 캐릭터 1개 조회
    server.Get("/characters/select", [&db_config, &jwt_config](const httplib::Request& req, httplib::Response& res) {

        json response_json;

        string token;
        if (!extract_bearer_token(req, token))
        {
            json error_json;
            error_json["result"] = "fail";
            error_json["message"] = "Missing or invalid Authorization header";

            res.status = 401;
            res.set_content(error_json.dump(), "application/json");
            return;
        }

        int user_id = 0;
        string username;
        string verify_error;

        if (!verify_jwt_token(token, jwt_config, user_id, username, verify_error))
        {
            json error_json;
            error_json["result"] = "fail";
            error_json["message"] = "Invalid token";
            error_json["detail"] = verify_error;

            res.status = 401;
            res.set_content(error_json.dump(), "application/json");
            return;
        }

        if (!req.has_param("character_id"))
        {
            res.status = 400;
            response_json["result"] = "fail";
            response_json["message"] = "Missing query parameter: character_id";
            res.set_content(response_json.dump(), "application/json");
            return;
        }

        int character_id = 0;
        try
        {
            character_id = stoi(req.get_param_value("character_id"));
        }
        catch (const exception&)
        {
            res.status = 400;
            response_json["result"] = "fail";
            response_json["message"] = "Invalid character_id";
            res.set_content(response_json.dump(), "application/json");
            return;
        }

        if (character_id <= 0)
        {
            res.status = 400;
            response_json["result"] = "fail";
            response_json["message"] = "character_id must be greater than 0";
            res.set_content(response_json.dump(), "application/json");
            return;
        }

        MYSQL* conn = connect_db(db_config);
        if (conn == nullptr)
        {
            json error_json;
            error_json["result"] = "error";
            error_json["message"] = "DB connection failed";

            res.status = 500;
            res.set_content(error_json.dump(), "application/json");
            return;
        }

        CharacterData character;
        string query_error;
        if (!get_character_by_id(conn, user_id, character_id, character, query_error))
        {
            mysql_close(conn);

            res.status = 404;
            response_json["result"] = "fail";
            response_json["message"] = "Character not found: " + query_error;
            res.set_content(response_json.dump(), "application/json");
            return;
        }

        mysql_close(conn);


        response_json["result"] = "success";
        response_json["message"] = "Character retrieved";
        response_json["character"] = {
            {"id", character.id},
            {"user_id", character.user_id},
            {"name", character.name},
            {"level", character.level},
            {"exp", character.exp},
            {"gold", character.gold},
            {"hp", character.hp},
            {"max_hp", character.max_hp},
            {"mp", character.mp},
            {"max_mp", character.max_mp},
            {"attack", character.attack},
            {"defense", character.defense},
            {"pos_x", character.pos_x},
            {"pos_y", character.pos_y},
            {"pos_z", character.pos_z},
            {"yaw", character.yaw},
            {"created_at", character.created_at},
            {"updated_at", character.updated_at}
        };

        res.status = 200;
        res.set_content(response_json.dump(), "application/json");

        });
    // 선택한 캐릭터 1개 삭제
    server.Post("/characters/delete", [&db_config, &jwt_config](const httplib::Request& req, httplib::Response& res) {

        json response_json;

        string token;
        if (!extract_bearer_token(req, token))
        {
            json error_json;
            error_json["result"] = "fail";
            error_json["message"] = "Missing or invalid Authorization header";

            res.status = 401;
            res.set_content(error_json.dump(), "application/json");
            return;
        }

        int user_id = 0;
        string username;
        string verify_error;

        if (!verify_jwt_token(token, jwt_config, user_id, username, verify_error))
        {
            json error_json;
            error_json["result"] = "fail";
            error_json["message"] = "Invalid token";
            error_json["detail"] = verify_error;

            res.status = 401;
            res.set_content(error_json.dump(), "application/json");
            return;
        }

        DeleteCharacterRequest delete_request;
        string parse_error;
        if (!parse_delete_character_request(req.body, delete_request, parse_error))
        {
            res.status = 400;
            response_json["result"] = "fail";
            response_json["message"] = "Invalid request body: " + parse_error;
            res.set_content(response_json.dump(), "application/json");
            return;
        }

        MYSQL* conn = connect_db(db_config);
        if (conn == nullptr)
        {
            json error_json;
            error_json["result"] = "error";
            error_json["message"] = "DB connection failed";

            res.status = 500;
            res.set_content(error_json.dump(), "application/json");
            return;
        }

        string delete_error;
        if (!delete_character_by_id(conn, user_id, delete_request.character_id, delete_error))
        {
            mysql_close(conn);

            res.status = 404;
            response_json["result"] = "fail";
            response_json["message"] = "Character delete failed: " + delete_error;
            res.set_content(response_json.dump(), "application/json");
            return;
        }

        mysql_close(conn);

        response_json["result"] = "success";
        response_json["message"] = "Character deleted";
        response_json["character_id"] = delete_request.character_id;

        res.status = 200;
        res.set_content(response_json.dump(), "application/json");
        });

    // 월드 입장
    server.Post("/world/enter", [&db_config, &jwt_config](const httplib::Request& req, httplib::Response& res)
        {
            json response_json;

            // 1. Authorization 헤더에서 Bearer 토큰 추출
            string token;
            if (!extract_bearer_token(req, token))
            {
                response_json["result"] = "fail";
                response_json["message"] = "Missing or invalid Authorization header";

                res.status = 401;
                res.set_content(response_json.dump(), "application/json");
                return;
            }

            // 2. JWT 검증
            int user_id = 0;
            string username;
            string verify_error;

            if (!verify_jwt_token(token, jwt_config, user_id, username, verify_error))
            {
                response_json["result"] = "fail";
                response_json["message"] = "Invalid token";
                response_json["detail"] = verify_error;

                res.status = 401;
                res.set_content(response_json.dump(), "application/json");
                return;
            }

            // 3. 요청 body 파싱
            WorldEnterRequest enter_request;
            string parse_error;

            if (!parse_world_enter_request(req.body, enter_request, parse_error))
            {
                response_json["result"] = "fail";
                response_json["message"] = "Invalid request body: " + parse_error;

                res.status = 400;
                res.set_content(response_json.dump(), "application/json");
                return;
            }

            // 4. DB 연결
            MYSQL* conn = connect_db(db_config);
            if (conn == nullptr)
            {
                response_json["result"] = "error";
                response_json["message"] = "DB connection failed";

                res.status = 500;
                res.set_content(response_json.dump(), "application/json");
                return;
            }

            // 5. 선택한 캐릭터 정보 조회
            CharacterData character;
            string query_error;

            if (!get_character_by_id(conn, user_id, enter_request.character_id, character, query_error))
            {
                mysql_close(conn);

                response_json["result"] = "fail";
                response_json["message"] = "Character not found: " + query_error;

                res.status = 404;
                res.set_content(response_json.dump(), "application/json");
                return;
            }

            mysql_close(conn);

            // 6. 현재 접속 중인 다른 플레이어 목록 수집
            //    자기 자신을 등록하기 전에 먼저 모아야 자기 자신이 nearby_players에 안 들어감
            json nearby_players = json::array();

            for (const auto& [online_character_id, online_character] : online_players)
            {
                if (online_character.character.id == character.id)
                {
                    continue;
                }

                nearby_players.push_back({
                    {"id", online_character.character.id},
                    {"user_id", online_character.character.user_id},
                    {"name", online_character.character.name},
                    {"level", online_character.character.level},
                    {"hp", online_character.character.hp},
                    {"max_hp", online_character.character.max_hp},
                    {"mp", online_character.character.mp},
                    {"max_mp", online_character.character.max_mp},
                    {"attack", online_character.character.attack},
                    {"defense", online_character.character.defense},
                    {"pos_x", online_character.character.pos_x},
                    {"pos_y", online_character.character.pos_y},
                    {"pos_z", online_character.character.pos_z},
                    {"yaw", online_character.character.yaw}
                    });
            }

            // 7. 현재 캐릭터를 온라인 목록에 등록
            //    이미 있으면 덮어씀
            online_players[character.id] = OnlinePlayerState{
                character,
                std::chrono::steady_clock::now()
            };

            // 8. 응답 반환
            response_json["result"] = "success";
            response_json["message"] = "Entered world successfully";

            response_json["character"] = {
                {"id", character.id},
                {"user_id", character.user_id},
                {"name", character.name},
                {"level", character.level},
                {"exp", character.exp},
                {"gold", character.gold},
                {"hp", character.hp},
                {"max_hp", character.max_hp},
                {"mp", character.mp},
                {"max_mp", character.max_mp},
                {"attack", character.attack},
                {"defense", character.defense},
                {"pos_x", character.pos_x},
                {"pos_y", character.pos_y},
                {"pos_z", character.pos_z},
                {"yaw", character.yaw},
                {"created_at", character.created_at},
                {"updated_at", character.updated_at}
            };

            response_json["nearby_players"] = nearby_players;

            res.status = 200;
            res.set_content(response_json.dump(), "application/json");
        });

        // 월드 내에서의 플레이어(나)의 좌표
        server.Post("/world/move", [&jwt_config](const httplib::Request& req, httplib::Response& res)
            {
                json response_json;

                // 1. Authorization 헤더 확인
                string token;
                if (!extract_bearer_token(req, token))
                {
                    response_json["result"] = "fail";
                    response_json["message"] = "Missing or invalid Authorization header";

                    res.status = 401;
                    res.set_content(response_json.dump(), "application/json");
                    return;
                }

                // 2. JWT 검증
                int user_id = 0;
                string username;
                string verify_error;

                if (!verify_jwt_token(token, jwt_config, user_id, username, verify_error))
                {
                    response_json["result"] = "fail";
                    response_json["message"] = "Invalid token";
                    response_json["detail"] = verify_error;

                    res.status = 401;
                    res.set_content(response_json.dump(), "application/json");
                    return;
                }

                // 3. body 파싱
                WorldMoveRequest move_request;
                string parse_error;

                if (!parse_world_move_request(req.body, move_request, parse_error))
                {
                    response_json["result"] = "fail";
                    response_json["message"] = "Invalid request body: " + parse_error;

                    res.status = 400;
                    res.set_content(response_json.dump(), "application/json");
                    return;
                }

                // 4. 현재 온라인 목록에서 해당 캐릭터 찾기
                auto it = online_players.find(move_request.character_id);
                if (it == online_players.end())
                {
                    response_json["result"] = "fail";
                    response_json["message"] = "Character is not currently in world";

                    res.status = 404;
                    res.set_content(response_json.dump(), "application/json");
                    return;
                }

                OnlinePlayerState& online_state = it->second;
                CharacterData& online_character = online_state.character;

                // 5. 자기 소유 캐릭터인지 확인
                if (online_character.user_id != user_id)
                {
                    response_json["result"] = "fail";
                    response_json["message"] = "You do not own this character";

                    res.status = 403;
                    res.set_content(response_json.dump(), "application/json");
                    return;
                }

                // 6. 위치 갱신
                online_character.pos_x = move_request.pos_x;
                online_character.pos_y = move_request.pos_y;
                online_character.pos_z = move_request.pos_z;
                online_character.yaw = move_request.yaw;

                online_state.last_seen = std::chrono::steady_clock::now();

                // 7. 성공 응답
                response_json["result"] = "success";
                response_json["message"] = "Character position updated";
                response_json["character_id"] = online_character.id;
                response_json["pos_x"] = online_character.pos_x;
                response_json["pos_y"] = online_character.pos_y;
                response_json["pos_z"] = online_character.pos_z;
                response_json["yaw"] = online_character.yaw;

                res.status = 200;
                res.set_content(response_json.dump(), "application/json");
            });

        // 월드 내에서 다른 플레이어들의 좌표
        server.Get("/world/players", [&jwt_config](const httplib::Request& req, httplib::Response& res)
            {

                json response_json;

                // 1. Authorization 헤더 확인
                string token;
                if (!extract_bearer_token(req, token))
                {
                    response_json["result"] = "fail";
                    response_json["message"] = "Missing or invalid Authorization header";

                    res.status = 401;
                    res.set_content(response_json.dump(), "application/json");
                    return;
                }

                // 2. JWT 검증
                int user_id = 0;
                string username;
                string verify_error;

                if (!verify_jwt_token(token, jwt_config, user_id, username, verify_error))
                {
                    response_json["result"] = "fail";
                    response_json["message"] = "Invalid token";
                    response_json["detail"] = verify_error;

                    res.status = 401;
                    res.set_content(response_json.dump(), "application/json");
                    return;
                }

                // 3. 현재 로그인한 유저가 월드에 들어와 있는 캐릭터를 찾기
                int my_character_id = 0;

                if (!find_online_character_by_user_id(user_id, my_character_id))
                {
                    response_json["result"] = "fail";
                    response_json["message"] = "No character currently entered in world";
                    res.status = 404;
                    res.set_content(response_json.dump(), "application/json");
                    return;
                }

                // 4. 살아있는 클라이언트이므로 heartbeat 갱신
                update_last_seen_by_character_id(my_character_id);

                // 5. 그 다음 오래된 플레이어 정리
                remove_timed_out_players();


                // 6. 다른 플레이어 목록 구성
                json players_json = json::array();

                for (const auto& [character_id, online_player] : online_players)
                {
                    if (character_id == my_character_id)
                    {
                        continue;
                    }

                    players_json.push_back({
                        {"id", online_player.character.id},
                        {"user_id", online_player.character.user_id},
                        {"name", online_player.character.name},
                        {"level", online_player.character.level},
                        {"hp", online_player.character.hp},
                        {"max_hp", online_player.character.max_hp},
                        {"mp", online_player.character.mp},
                        {"max_mp", online_player.character.max_mp},
                        {"attack", online_player.character.attack},
                        {"defense", online_player.character.defense},
                        {"pos_x", online_player.character.pos_x},
                        {"pos_y", online_player.character.pos_y},
                        {"pos_z", online_player.character.pos_z},
                        {"yaw", online_player.character.yaw},
                        });
                }

                // 5. 응답 반환
                response_json["result"] = "success";
                response_json["message"] = "Online player list retrieved";
                response_json["players"] = players_json;

                res.status = 200;
                res.set_content(response_json.dump(), "application/json");
            });

        // 월드 내에서 퇴장
        server.Post("/world/leave", [&jwt_config](const httplib::Request& req, httplib::Response& res)
            {
                json response_json;

                // 1. Authorization 헤더 확인
                string token;
                if (!extract_bearer_token(req, token))
                {
                    response_json["result"] = "fail";
                    response_json["message"] = "Missing or invalid Authorization header";

                    res.status = 401;
                    res.set_content(response_json.dump(), "application/json");
                    return;
                }

                // 2. JWT 검증
                int user_id = 0;
                string username;
                string verify_error;

                if (!verify_jwt_token(token, jwt_config, user_id, username, verify_error))
                {
                    response_json["result"] = "fail";
                    response_json["message"] = "Invalid token";
                    response_json["detail"] = verify_error;

                    res.status = 401;
                    res.set_content(response_json.dump(), "application/json");
                    return;
                }

                // 3. 요청 body 파싱
                WorldLeaveRequest leave_request;
                string parse_error;

                if (!parse_world_leave_request(req.body, leave_request, parse_error))
                {
                    response_json["result"] = "fail";
                    response_json["message"] = "Invalid request body: " + parse_error;

                    res.status = 400;
                    res.set_content(response_json.dump(), "application/json");
                    return;
                }

                // 4. 온라인 목록에서 해당 캐릭터 찾기
                auto it = online_players.find(leave_request.character_id);
                if (it == online_players.end())
                {
                    response_json["result"] = "fail";
                    response_json["message"] = "Character is not currently in world";

                    res.status = 404;
                    res.set_content(response_json.dump(), "application/json");
                    return;
                }

                // 5. 자기 소유 캐릭터인지 확인
                if (it->second.character.user_id != user_id)
                {
                    response_json["result"] = "fail";
                    response_json["message"] = "You do not own this character";

                    res.status = 403;
                    res.set_content(response_json.dump(), "application/json");
                    return;
                }

                string character_name = it->second.character.name;

                // 6. 온라인 목록에서 제거
                online_players.erase(it);

                // 7. 성공 응답
                response_json["result"] = "success";
                response_json["message"] = "Left world successfully";
                response_json["character_id"] = leave_request.character_id;
                response_json["character_name"] = character_name;

                res.status = 200;
                res.set_content(response_json.dump(), "application/json");
            });
    cout << "Server started on port 8080" << endl;
    server.listen("0.0.0.0", 8080);

    return 0;
}