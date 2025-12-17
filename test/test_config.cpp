#include <cassert>
#include <iostream>
#include <string>

#include "Config.hpp"

// ============================================================================
// テストヘルパー
// ============================================================================

static int g_test_count = 0;
static int g_pass_count = 0;

#define TEST(name)                              \
  std::cout << "  Testing: " << name << "... "; \
  g_test_count++;

#define PASS()                    \
  std::cout << "OK" << std::endl; \
  g_pass_count++;

#define ASSERT_EQ(expected, actual)                           \
  if ((expected) != (actual)) {                               \
    std::cout << "FAILED" << std::endl;                       \
    std::cerr << "    Expected: " << (expected) << std::endl; \
    std::cerr << "    Actual:   " << (actual) << std::endl;   \
    return;                                                   \
  }

#define ASSERT_TRUE(condition)                           \
  if (!(condition)) {                                    \
    std::cout << "FAILED" << std::endl;                  \
    std::cerr << "    Condition was false" << std::endl; \
    return;                                              \
  }

#define ASSERT_NULL(ptr)                                            \
  if ((ptr) != NULL) {                                              \
    std::cout << "FAILED" << std::endl;                             \
    std::cerr << "    Expected NULL but got non-NULL" << std::endl; \
    return;                                                         \
  }

#define ASSERT_NOT_NULL(ptr)                                        \
  if ((ptr) == NULL) {                                              \
    std::cout << "FAILED" << std::endl;                             \
    std::cerr << "    Expected non-NULL but got NULL" << std::endl; \
    return;                                                         \
  }

// ============================================================================
// LocationConfig Tests
// ============================================================================

void test_location_config_defaults() {
  TEST("LocationConfig default values");

  LocationConfig loc;

  ASSERT_EQ("/", loc.path);
  ASSERT_EQ("", loc.root);
  ASSERT_EQ("", loc.alias);
  ASSERT_EQ("index.html", loc.index);
  ASSERT_EQ("", loc.cgi_extension);
  ASSERT_EQ("", loc.cgi_path);
  ASSERT_EQ("", loc.upload_path);
  ASSERT_EQ(false, loc.autoindex);
  ASSERT_EQ(0, loc.return_redirect.first);
  ASSERT_EQ("", loc.return_redirect.second);
  ASSERT_EQ(1u, loc.allow_methods.size());
  ASSERT_EQ(GET, loc.allow_methods[0]);

  PASS();
}

// ============================================================================
// ServerConfig Tests
// ============================================================================

void test_server_config_defaults() {
  TEST("ServerConfig default values");

  ServerConfig server;

  ASSERT_EQ(80, server.listen_port);
  ASSERT_EQ("0.0.0.0", server.host);
  ASSERT_EQ(DEFAULT_CLIENT_MAX_BODY_SIZE, server.client_max_body_size);
  ASSERT_TRUE(server.server_names.empty());
  ASSERT_TRUE(server.error_pages.empty());
  ASSERT_TRUE(server.locations.empty());

  PASS();
}

void test_get_location_exact_match() {
  TEST("getLocation exact match");

  ServerConfig server;

  LocationConfig loc1;
  loc1.path = "/";
  loc1.root = "www";

  LocationConfig loc2;
  loc2.path = "/api";
  loc2.root = "www/api";

  server.locations.push_back(loc1);
  server.locations.push_back(loc2);

  const LocationConfig* result = server.getLocation("/api");
  ASSERT_NOT_NULL(result);
  ASSERT_EQ("/api", result->path);
  ASSERT_EQ("www/api", result->root);

  PASS();
}

void test_get_location_prefix_match() {
  TEST("getLocation prefix match");

  ServerConfig server;

  LocationConfig loc1;
  loc1.path = "/";
  loc1.root = "www";

  LocationConfig loc2;
  loc2.path = "/api";
  loc2.root = "www/api";

  server.locations.push_back(loc1);
  server.locations.push_back(loc2);

  // /api/users は /api にマッチすべき
  const LocationConfig* result = server.getLocation("/api/users");
  ASSERT_NOT_NULL(result);
  ASSERT_EQ("/api", result->path);

  PASS();
}

void test_get_location_no_false_prefix() {
  TEST("getLocation no false prefix (/foo vs /foobar)");

  ServerConfig server;

  LocationConfig loc1;
  loc1.path = "/";
  loc1.root = "www";

  LocationConfig loc2;
  loc2.path = "/foo";
  loc2.root = "www/foo";

  server.locations.push_back(loc1);
  server.locations.push_back(loc2);

  // /foobar は /foo にマッチしてはいけない、/ にマッチすべき
  const LocationConfig* result = server.getLocation("/foobar");
  ASSERT_NOT_NULL(result);
  ASSERT_EQ("/", result->path);

  PASS();
}

void test_get_location_longest_match() {
  TEST("getLocation longest match");

  ServerConfig server;

  LocationConfig loc1;
  loc1.path = "/";
  loc1.root = "www";

  LocationConfig loc2;
  loc2.path = "/api";
  loc2.root = "www/api";

  LocationConfig loc3;
  loc3.path = "/api/v1";
  loc3.root = "www/api/v1";

  server.locations.push_back(loc1);
  server.locations.push_back(loc2);
  server.locations.push_back(loc3);

  // /api/v1/users は /api/v1 にマッチすべき（最長）
  const LocationConfig* result = server.getLocation("/api/v1/users");
  ASSERT_NOT_NULL(result);
  ASSERT_EQ("/api/v1", result->path);

  PASS();
}

void test_get_location_root_fallback() {
  TEST("getLocation root fallback");

  ServerConfig server;

  LocationConfig loc1;
  loc1.path = "/";
  loc1.root = "www";

  LocationConfig loc2;
  loc2.path = "/api";
  loc2.root = "www/api";

  server.locations.push_back(loc1);
  server.locations.push_back(loc2);

  // /unknown は / にフォールバックすべき
  const LocationConfig* result = server.getLocation("/unknown");
  ASSERT_NOT_NULL(result);
  ASSERT_EQ("/", result->path);

  PASS();
}

void test_get_location_no_root_returns_null() {
  TEST("getLocation no root returns NULL");

  ServerConfig server;

  LocationConfig loc1;
  loc1.path = "/api";
  loc1.root = "www/api";

  server.locations.push_back(loc1);

  // / のlocationがない場合、/unknown は NULL を返す
  const LocationConfig* result = server.getLocation("/unknown");
  ASSERT_NULL(result);

  PASS();
}

// ============================================================================
// MainConfig Tests
// ============================================================================

void test_main_config_defaults() {
  TEST("MainConfig default values");

  MainConfig config;

  ASSERT_TRUE(config.servers.empty());

  PASS();
}

void test_get_server_exact_match() {
  TEST("getServer exact server_name match");

  MainConfig config;

  ServerConfig server1;
  server1.listen_port = 8080;
  server1.server_names.push_back("example.com");

  ServerConfig server2;
  server2.listen_port = 8080;
  server2.server_names.push_back("test.com");

  config.servers.push_back(server1);
  config.servers.push_back(server2);

  const ServerConfig* result = config.getServer("test.com", 8080);
  ASSERT_NOT_NULL(result);
  ASSERT_EQ("test.com", result->server_names[0]);

  PASS();
}

void test_get_server_case_insensitive() {
  TEST("getServer case insensitive");

  MainConfig config;

  ServerConfig server;
  server.listen_port = 8080;
  server.server_names.push_back("example.com");

  config.servers.push_back(server);

  // 大文字でもマッチすべき
  const ServerConfig* result = config.getServer("EXAMPLE.COM", 8080);
  ASSERT_NOT_NULL(result);
  ASSERT_EQ("example.com", result->server_names[0]);

  PASS();
}

void test_get_server_strip_port() {
  TEST("getServer strips port from host");

  MainConfig config;

  ServerConfig server;
  server.listen_port = 8080;
  server.server_names.push_back("example.com");

  config.servers.push_back(server);

  // Host:port 形式でもマッチすべき
  const ServerConfig* result = config.getServer("example.com:8080", 8080);
  ASSERT_NOT_NULL(result);
  ASSERT_EQ("example.com", result->server_names[0]);

  PASS();
}

void test_get_server_strip_trailing_dot() {
  TEST("getServer strips trailing dot");

  MainConfig config;

  ServerConfig server;
  server.listen_port = 8080;
  server.server_names.push_back("example.com");

  config.servers.push_back(server);

  // 末尾ドット付きでもマッチすべき
  const ServerConfig* result = config.getServer("example.com.", 8080);
  ASSERT_NOT_NULL(result);
  ASSERT_EQ("example.com", result->server_names[0]);

  PASS();
}

void test_get_server_default_for_port() {
  TEST("getServer default server for port");

  MainConfig config;

  ServerConfig server1;
  server1.listen_port = 8080;
  server1.server_names.push_back("first.com");

  ServerConfig server2;
  server2.listen_port = 8080;
  server2.server_names.push_back("second.com");

  config.servers.push_back(server1);
  config.servers.push_back(server2);

  // unknown.com は同じポートの最初のサーバーにフォールバック
  const ServerConfig* result = config.getServer("unknown.com", 8080);
  ASSERT_NOT_NULL(result);
  ASSERT_EQ("first.com", result->server_names[0]);

  PASS();
}

void test_get_server_fallback_to_first() {
  TEST("getServer fallback to first server");

  MainConfig config;

  ServerConfig server1;
  server1.listen_port = 8080;
  server1.server_names.push_back("example.com");

  ServerConfig server2;
  server2.listen_port = 9000;
  server2.server_names.push_back("api.com");

  config.servers.push_back(server1);
  config.servers.push_back(server2);

  // ポートが一致しない場合、最初のサーバーにフォールバック
  const ServerConfig* result = config.getServer("unknown.com", 3000);
  ASSERT_NOT_NULL(result);
  ASSERT_EQ("example.com", result->server_names[0]);

  PASS();
}

void test_get_server_empty_returns_null() {
  TEST("getServer empty servers returns NULL");

  MainConfig config;

  const ServerConfig* result = config.getServer("example.com", 8080);
  ASSERT_NULL(result);

  PASS();
}

// ============================================================================
// Main
// ============================================================================

int main() {
  std::cout << std::endl;
  std::cout << "========================================" << std::endl;
  std::cout << "Config Tests" << std::endl;
  std::cout << "========================================" << std::endl;

  std::cout << std::endl << "[LocationConfig]" << std::endl;
  test_location_config_defaults();

  std::cout << std::endl << "[ServerConfig]" << std::endl;
  test_server_config_defaults();
  test_get_location_exact_match();
  test_get_location_prefix_match();
  test_get_location_no_false_prefix();
  test_get_location_longest_match();
  test_get_location_root_fallback();
  test_get_location_no_root_returns_null();

  std::cout << std::endl << "[MainConfig]" << std::endl;
  test_main_config_defaults();
  test_get_server_exact_match();
  test_get_server_case_insensitive();
  test_get_server_strip_port();
  test_get_server_strip_trailing_dot();
  test_get_server_default_for_port();
  test_get_server_fallback_to_first();
  test_get_server_empty_returns_null();

  std::cout << std::endl;
  std::cout << "========================================" << std::endl;
  std::cout << "Results: " << g_pass_count << "/" << g_test_count << " passed";
  if (g_pass_count == g_test_count) {
    std::cout << " [PASS]" << std::endl;
  } else {
    std::cout << " [FAIL]" << std::endl;
  }
  std::cout << "========================================" << std::endl;
  std::cout << std::endl;

  return (g_pass_count == g_test_count) ? 0 : 1;
}
