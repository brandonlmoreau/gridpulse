#pragma once

#include <string>
#include <optional>
#include <chrono>

namespace gridpulse {

struct TokenPayload {
    int user_id;
    std::string username;
    std::chrono::system_clock::time_point expires_at;
};

class Auth {
public:
    // Password hashing
    static std::string hashPassword(const std::string& password);
    static bool verifyPassword(const std::string& password, const std::string& hash);
    
    // JWT token operations
    static std::string generateToken(int user_id, const std::string& username);
    static std::optional<TokenPayload> verifyToken(const std::string& token);
    
    // Token extraction from header
    static std::optional<std::string> extractBearerToken(const std::string& auth_header);
    
private:
    static const std::string JWT_SECRET;
    static const int TOKEN_EXPIRY_HOURS = 24;
};

} // namespace gridpulse
