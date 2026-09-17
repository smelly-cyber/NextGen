# Validation + account server. Uses the single-header cpp-httplib and
# nlohmann/json vendored under server/thirdparty, plus the shared licence core.
add_executable(ngtauthd main.cpp)

target_include_directories(ngtauthd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty)
target_link_libraries(ngtauthd PRIVATE nglicense OpenSSL::Crypto)

# The server speaks plain HTTP and is meant to run behind a TLS-terminating
# reverse proxy (Caddy / Cloudflare Tunnel / nginx). httplib's own TLS needs
# OpenSSL 3.0+, which the licence crypto (1.1.1) does not, so it stays off.

if(WIN32)
    target_link_libraries(ngtauthd PRIVATE ws2_32 crypt32)
endif()
