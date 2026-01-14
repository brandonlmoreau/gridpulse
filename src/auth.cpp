#include "gridpulse/auth.hpp"
#include <jwt-cpp/jwt.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <sstream>
#include <iomanip>
#include <random>

namespace gridpulse {

// Secret key for JWT signing (in production, use environment variable)
const std::string Auth::JWT_SECRET = "gridpulse_jwt_secret_key_2024_secure";

std::string Auth::hashPassword(const std::string& password) {
    // Generate a random salt
    unsigned char salt[16];
    RAND_bytes(salt, sizeof(salt));
    
    // Convert salt to hex string
    std::stringstream salt_ss;
    for (int i = 0; i < 16; i++) {
        salt_ss << std::hex << std::setw(2) << std::setfill('0') << (int)salt[i];
    }
    std::string salt_hex = salt_ss.str();
    
    // Combine salt and password
    std::string salted = salt_hex + password;
    
    // Hash with SHA256
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(salted.c_str()), salted.size(), hash);
    
    // Convert hash to hex string
    std::stringstream hash_ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        hash_ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    
    // Return salt:hash format
    return salt_hex + ":" + hash_ss.str();
}

bool Auth::verifyPassword(const std::string& password, const std::string& stored_hash) {
    // Extract salt from stored hash
    auto colon_pos = stored_hash.find(':');
    if (colon_pos == std::string::npos) {
        return false;
    }
    
    std::string salt_hex = stored_hash.substr(0, colon_pos);
    std::string expected_hash = stored_hash.substr(colon_pos + 1);
    
    // Combine salt and password
    std::string salted = salt_hex + password;
    
    // Hash with SHA256
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(salted.c_str()), salted.size(), hash);
    
    // Convert hash to hex string
    std::stringstream hash_ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        hash_ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    
    return hash_ss.str() == expected_hash;
}

std::string Auth::generateToken(int user_id, const std::string& username) {
    auto now = std::chrono::system_clock::now();
    auto expires = now + std::chrono::hours(TOKEN_EXPIRY_HOURS);
    
    auto token = jwt::create()
        .set_issuer("gridpulse")
        .set_subject(std::to_string(user_id))
        .set_payload_claim("username", jwt::claim(username))
        .set_issued_at(now)
        .set_expires_at(expires)
        .sign(jwt::algorithm::hs256{JWT_SECRET});
    
    return token;
}

std::optional<TokenPayload> Auth::verifyToken(const std::string& token) {
    try {
        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{JWT_SECRET})
            .with_issuer("gridpulse");
        
        auto decoded = jwt::decode(token);
        verifier.verify(decoded);
        
        TokenPayload payload;
        payload.user_id = std::stoi(decoded.get_subject());
        payload.username = decoded.get_payload_claim("username").as_string();
        payload.expires_at = decoded.get_expires_at();
        
        return payload;
    } catch (const std::exception& e) {
        return std::nullopt;
    }
}

std::optional<std::string> Auth::extractBearerToken(const std::string& auth_header) {
    const std::string prefix = "Bearer ";
    if (auth_header.size() > prefix.size() && 
        auth_header.substr(0, prefix.size()) == prefix) {
        return auth_header.substr(prefix.size());
    }
    return std::nullopt;
}

} // namespace gridpulse
