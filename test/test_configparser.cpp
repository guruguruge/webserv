#include <fstream>
#include <iostream>
#include "Config.hpp"
#include "ConfigParser.hpp"

// ============================================================================
// テストユーティリティ
// ============================================================================

static int g_test_count = 0;
static int g_pass_count = 0;

#define TEST(name)                                \
  do {                                            \
    ++g_test_count;                               \
    std::cout << "  Testing: " << name << "... "; \
  } while (0)

#define PASS()                      \
  do {                              \
    ++g_pass_count;                 \
    std::cout << "OK" << std::endl; \
  } while (0)

#define FAIL(msg)                              \
  do {                                         \
    std::cout << "FAIL: " << msg << std::endl; \
    return;                                    \
  } while (0)

#define ASSERT_EQ(expected, actual)   \
  do {                                \
    if ((expected) != (actual))       \
      FAIL(#actual " != " #expected); \
  } while (0)

#define ASSERT_TRUE(cond)      \
  do {                         \
    if (!(cond))               \
      FAIL(#cond " is false"); \
  } while (0)

// ============================================================================
// テストケース
// ============================================================================

void test_parse_basic_server() {
  TEST("parse basic server block");

  // テスト用設定ファイルを作成
  const char* test_conf = "/tmp/test_basic.conf";
  std::ofstream file(test_conf);
  file << "server {\n";
  file << "    listen 8080;\n";
  file << "    server_name localhost;\n";
  file << "}\n";
  file.close();

  MainConfig config;
  ConfigParser parser(test_conf);
  parser.parse(config);

  ASSERT_EQ(1u, config.servers.size());
  ASSERT_EQ(8080, config.servers[0].listen_port);
  ASSERT_EQ(1u, config.servers[0].server_names.size());
  ASSERT_EQ("localhost", config.servers[0].server_names[0]);

  PASS();
}

void test_parse_location() {
  TEST("parse location block");

  const char* test_conf = "/tmp/test_location.conf";
  std::ofstream file(test_conf);
  file << "server {\n";
  file << "    listen 8080;\n";
  file << "    location / {\n";
  file << "        root www;\n";
  file << "        index index.html;\n";
  file << "        autoindex off;\n";
  file << "    }\n";
  file << "}\n";
  file.close();

  MainConfig config;
  ConfigParser parser(test_conf);
  parser.parse(config);

  ASSERT_EQ(1u, config.servers.size());
  ASSERT_EQ(1u, config.servers[0].locations.size());

  const LocationConfig& loc = config.servers[0].locations[0];
  ASSERT_EQ("/", loc.path);
  ASSERT_EQ("www", loc.root);
  ASSERT_EQ("index.html", loc.index);
  ASSERT_EQ(false, loc.autoindex);

  PASS();
}

void test_parse_allowed_methods() {
  TEST("parse allowed_methods directive");

  const char* test_conf = "/tmp/test_methods.conf";
  std::ofstream file(test_conf);
  file << "server {\n";
  file << "    listen 8080;\n";
  file << "    location /api {\n";
  file << "        root www;\n";
  file << "        allowed_methods GET POST DELETE;\n";
  file << "    }\n";
  file << "}\n";
  file.close();

  MainConfig config;
  ConfigParser parser(test_conf);
  parser.parse(config);

  const LocationConfig& loc = config.servers[0].locations[0];
  ASSERT_EQ(3u, loc.allow_methods.size());
  ASSERT_EQ(GET, loc.allow_methods[0]);
  ASSERT_EQ(POST, loc.allow_methods[1]);
  ASSERT_EQ(DELETE, loc.allow_methods[2]);

  PASS();
}

void test_parse_error_page() {
  TEST("parse error_page directive");

  const char* test_conf = "/tmp/test_errorpage.conf";
  std::ofstream file(test_conf);
  file << "server {\n";
  file << "    listen 8080;\n";
  file << "    error_page 404 /404.html;\n";
  file << "    error_page 500 502 503 /50x.html;\n";
  file << "}\n";
  file.close();

  MainConfig config;
  ConfigParser parser(test_conf);
  parser.parse(config);

  ASSERT_EQ("/404.html", config.servers[0].error_pages[404]);
  ASSERT_EQ("/50x.html", config.servers[0].error_pages[500]);
  ASSERT_EQ("/50x.html", config.servers[0].error_pages[502]);
  ASSERT_EQ("/50x.html", config.servers[0].error_pages[503]);

  PASS();
}

void test_parse_client_max_body_size() {
  TEST("parse client_max_body_size directive");

  const char* test_conf = "/tmp/test_bodysize.conf";
  std::ofstream file(test_conf);
  file << "server {\n";
  file << "    listen 8080;\n";
  file << "    client_max_body_size 10M;\n";
  file << "}\n";
  file.close();

  MainConfig config;
  ConfigParser parser(test_conf);
  parser.parse(config);

  ASSERT_EQ(10u * 1024 * 1024, config.servers[0].client_max_body_size);

  PASS();
}

void test_parse_cgi() {
  TEST("parse cgi directives");

  const char* test_conf = "/tmp/test_cgi.conf";
  std::ofstream file(test_conf);
  file << "server {\n";
  file << "    listen 8080;\n";
  file << "    location /cgi-bin {\n";
  file << "        root www;\n";
  file << "        cgi_extension .py;\n";
  file << "        cgi_path /usr/bin/python3;\n";
  file << "    }\n";
  file << "}\n";
  file.close();

  MainConfig config;
  ConfigParser parser(test_conf);
  parser.parse(config);

  const LocationConfig& loc = config.servers[0].locations[0];
  ASSERT_EQ(".py", loc.cgi_extension);
  ASSERT_EQ("/usr/bin/python3", loc.cgi_path);

  PASS();
}

void test_parse_return() {
  TEST("parse return directive");

  const char* test_conf = "/tmp/test_return.conf";
  std::ofstream file(test_conf);
  file << "server {\n";
  file << "    listen 8080;\n";
  file << "    location /old {\n";
  file << "        return 301 http://example.com/new;\n";
  file << "    }\n";
  file << "}\n";
  file.close();

  MainConfig config;
  ConfigParser parser(test_conf);
  parser.parse(config);

  const LocationConfig& loc = config.servers[0].locations[0];
  ASSERT_EQ(301, loc.return_redirect.first);
  ASSERT_EQ("http://example.com/new", loc.return_redirect.second);

  PASS();
}

void test_parse_comment() {
  TEST("parse with comments");

  const char* test_conf = "/tmp/test_comment.conf";
  std::ofstream file(test_conf);
  file << "# This is a comment\n";
  file << "server {\n";
  file << "    listen 8080; # inline comment\n";
  file << "    # another comment\n";
  file << "    server_name localhost;\n";
  file << "}\n";
  file.close();

  MainConfig config;
  ConfigParser parser(test_conf);
  parser.parse(config);

  ASSERT_EQ(1u, config.servers.size());
  ASSERT_EQ(8080, config.servers[0].listen_port);

  PASS();
}

void test_parse_multiple_servers() {
  TEST("parse multiple server blocks");

  const char* test_conf = "/tmp/test_multi.conf";
  std::ofstream file(test_conf);
  file << "server {\n";
  file << "    listen 8080;\n";
  file << "    server_name example.com;\n";
  file << "}\n";
  file << "server {\n";
  file << "    listen 9000;\n";
  file << "    server_name api.example.com;\n";
  file << "}\n";
  file.close();

  MainConfig config;
  ConfigParser parser(test_conf);
  parser.parse(config);

  ASSERT_EQ(2u, config.servers.size());
  ASSERT_EQ(8080, config.servers[0].listen_port);
  ASSERT_EQ(9000, config.servers[1].listen_port);
  ASSERT_EQ("example.com", config.servers[0].server_names[0]);
  ASSERT_EQ("api.example.com", config.servers[1].server_names[0]);

  PASS();
}

void test_parse_host_port() {
  TEST("parse listen with host:port");

  const char* test_conf = "/tmp/test_hostport.conf";
  std::ofstream file(test_conf);
  file << "server {\n";
  file << "    listen 127.0.0.1:8080;\n";
  file << "}\n";
  file.close();

  MainConfig config;
  ConfigParser parser(test_conf);
  parser.parse(config);

  ASSERT_EQ("127.0.0.1", config.servers[0].host);
  ASSERT_EQ(8080, config.servers[0].listen_port);

  PASS();
}

void test_server_name_normalization() {
  TEST("server_name is normalized to lowercase");

  const char* test_conf = "/tmp/test_servername_norm.conf";
  std::ofstream file(test_conf);
  file << "server {\n";
  file << "    listen 8080;\n";
  file << "    server_name EXAMPLE.COM WWW.Example.ORG;\n";
  file << "}\n";
  file.close();

  MainConfig config;
  ConfigParser parser(test_conf);
  parser.parse(config);

  ASSERT_EQ(2u, config.servers[0].server_names.size());
  ASSERT_EQ("example.com", config.servers[0].server_names[0]);
  ASSERT_EQ("www.example.org", config.servers[0].server_names[1]);

  PASS();
}

void test_duplicate_listen_error() {
  TEST("duplicate listen directive throws error");

  const char* test_conf = "/tmp/test_dup_listen.conf";
  std::ofstream file(test_conf);
  file << "server {\n";
  file << "    listen 8080;\n";
  file << "    listen 8081;\n";
  file << "}\n";
  file.close();

  MainConfig config;
  ConfigParser parser(test_conf);

  bool caught = false;
  try {
    parser.parse(config);
  } catch (const std::runtime_error& e) {
    caught = true;
    // エラーメッセージに "duplicate" が含まれていることを確認
    std::string msg = e.what();
    ASSERT_TRUE(msg.find("duplicate") != std::string::npos);
  }
  ASSERT_TRUE(caught);

  PASS();
}

void test_duplicate_location_error() {
  TEST("duplicate location path throws error");

  const char* test_conf = "/tmp/test_dup_location.conf";
  std::ofstream file(test_conf);
  file << "server {\n";
  file << "    listen 8080;\n";
  file << "    location /api {\n";
  file << "        root www;\n";
  file << "    }\n";
  file << "    location /api {\n";
  file << "        root www2;\n";
  file << "    }\n";
  file << "}\n";
  file.close();

  MainConfig config;
  ConfigParser parser(test_conf);

  bool caught = false;
  try {
    parser.parse(config);
  } catch (const std::runtime_error& e) {
    caught = true;
    std::string msg = e.what();
    ASSERT_TRUE(msg.find("duplicate") != std::string::npos);
  }
  ASSERT_TRUE(caught);

  PASS();
}

void test_error_has_line_number() {
  TEST("error message includes line number");

  const char* test_conf = "/tmp/test_lineno.conf";
  std::ofstream file(test_conf);
  file << "server {\n";                // line 1
  file << "    listen 8080;\n";        // line 2
  file << "    unknown_directive;\n";  // line 3 - error
  file << "}\n";
  file.close();

  MainConfig config;
  ConfigParser parser(test_conf);

  bool caught = false;
  try {
    parser.parse(config);
  } catch (const std::runtime_error& e) {
    caught = true;
    std::string msg = e.what();
    // エラーメッセージに行番号 "3" が含まれていることを確認
    ASSERT_TRUE(msg.find(":3:") != std::string::npos);
  }
  ASSERT_TRUE(caught);

  PASS();
}

void test_return_invalid_status() {
  TEST("return with invalid status code throws error");

  const char* test_conf = "/tmp/test_return_invalid.conf";
  std::ofstream file(test_conf);
  file << "server {\n";
  file << "    listen 8080;\n";
  file << "    location /old {\n";
  file << "        return 200 http://example.com;\n";  // 200は不正
  file << "    }\n";
  file << "}\n";
  file.close();

  MainConfig config;
  ConfigParser parser(test_conf);

  bool caught = false;
  try {
    parser.parse(config);
  } catch (const std::runtime_error& e) {
    caught = true;
    std::string msg = e.what();
    ASSERT_TRUE(msg.find("300-399") != std::string::npos);
  }
  ASSERT_TRUE(caught);

  PASS();
}

void test_server_name_trailing_dot() {
  TEST("server_name trailing dot is removed");

  const char* test_conf = "/tmp/test_servername_dot.conf";
  std::ofstream file(test_conf);
  file << "server {\n";
  file << "    listen 8080;\n";
  file << "    server_name example.com.;\n";
  file << "}\n";
  file.close();

  MainConfig config;
  ConfigParser parser(test_conf);
  parser.parse(config);

  ASSERT_EQ(1u, config.servers[0].server_names.size());
  ASSERT_EQ("example.com", config.servers[0].server_names[0]);

  PASS();
}

void test_error_page_requires_code() {
  TEST("error_page without code throws error");

  const char* test_conf = "/tmp/test_errorpage_nocode.conf";
  std::ofstream file(test_conf);
  file << "server {\n";
  file << "    listen 8080;\n";
  file << "    error_page /error.html;\n";
  file << "}\n";
  file.close();

  MainConfig config;
  ConfigParser parser(test_conf);

  bool caught = false;
  try {
    parser.parse(config);
  } catch (const std::runtime_error& e) {
    caught = true;
    std::string msg = e.what();
    ASSERT_TRUE(msg.find("status code") != std::string::npos);
  }
  ASSERT_TRUE(caught);

  PASS();
}

void test_ipv6_not_supported() {
  TEST("IPv6 listen throws error");

  const char* test_conf = "/tmp/test_ipv6.conf";
  std::ofstream file(test_conf);
  file << "server {\n";
  file << "    listen [::1]:8080;\n";
  file << "}\n";
  file.close();

  MainConfig config;
  ConfigParser parser(test_conf);

  bool caught = false;
  try {
    parser.parse(config);
  } catch (const std::runtime_error& e) {
    caught = true;
    std::string msg = e.what();
    ASSERT_TRUE(msg.find("IPv6") != std::string::npos);
  }
  ASSERT_TRUE(caught);

  PASS();
}

void test_duplicate_return_error() {
  TEST("duplicate return directive throws error");

  const char* test_conf = "/tmp/test_dup_return.conf";
  std::ofstream file(test_conf);
  file << "server {\n";
  file << "    listen 8080;\n";
  file << "    location /old {\n";
  file << "        return 301 http://a.com;\n";
  file << "        return 302 http://b.com;\n";
  file << "    }\n";
  file << "}\n";
  file.close();

  MainConfig config;
  ConfigParser parser(test_conf);

  bool caught = false;
  try {
    parser.parse(config);
  } catch (const std::runtime_error& e) {
    caught = true;
    std::string msg = e.what();
    ASSERT_TRUE(msg.find("duplicate") != std::string::npos);
  }
  ASSERT_TRUE(caught);

  PASS();
}

void test_allowed_methods_dedup() {
  TEST("duplicate methods in allowed_methods are deduplicated");

  const char* test_conf = "/tmp/test_methods_dedup.conf";
  std::ofstream file(test_conf);
  file << "server {\n";
  file << "    listen 8080;\n";
  file << "    location / {\n";
  file << "        root www;\n";
  file << "        allowed_methods GET GET POST GET;\n";
  file << "    }\n";
  file << "}\n";
  file.close();

  MainConfig config;
  ConfigParser parser(test_conf);
  parser.parse(config);

  const LocationConfig& loc = config.servers[0].locations[0];
  // GET, POST の2つだけになるはず
  ASSERT_EQ(2u, loc.allow_methods.size());
  ASSERT_EQ(GET, loc.allow_methods[0]);
  ASSERT_EQ(POST, loc.allow_methods[1]);

  PASS();
}

// Test: listen directive is required
void test_listen_required() {
  TEST("listen directive is required in server block");

  const char* test_conf = "/tmp/test_listen_required.conf";
  std::ofstream file(test_conf);
  file << "server {\n";
  file << "    server_name localhost;\n";  // listenなし
  file << "}\n";
  file.close();

  MainConfig config;
  ConfigParser parser(test_conf);
  try {
    parser.parse(config);
    FAIL("should throw on missing listen directive");
  } catch (const std::runtime_error& e) {
    std::string msg(e.what());
    ASSERT_TRUE(msg.find("'listen' directive is required") !=
                std::string::npos);
  }

  PASS();
}

// Test: error_page with invalid status code range
void test_error_page_invalid_code_range() {
  TEST("error_page with invalid status code (outside 100-599)");

  const char* test_conf = "/tmp/test_error_page_range.conf";
  std::ofstream file(test_conf);
  file << "server {\n";
  file << "    listen 8080;\n";
  file << "    error_page 999 /error.html;\n";  // 範囲外
  file << "}\n";
  file.close();

  MainConfig config;
  ConfigParser parser(test_conf);
  try {
    parser.parse(config);
    FAIL("should throw on invalid status code 999");
  } catch (const std::runtime_error& e) {
    std::string msg(e.what());
    ASSERT_TRUE(msg.find("invalid status code") != std::string::npos ||
                msg.find("100-599") != std::string::npos);
  }

  PASS();
}

// Test: listen with multiple colons throws error
void test_listen_multiple_colons() {
  TEST("listen with multiple colons throws error");

  const char* test_conf = "/tmp/test_listen_colons.conf";
  std::ofstream file(test_conf);
  file << "server {\n";
  file << "    listen localhost:8080:123;\n";
  file << "}\n";
  file.close();

  MainConfig config;
  ConfigParser parser(test_conf);
  try {
    parser.parse(config);
    FAIL("should throw on multiple colons");
  } catch (const std::runtime_error& e) {
    std::string msg(e.what());
    ASSERT_TRUE(msg.find("multiple colons") != std::string::npos);
  }

  PASS();
}

// Test: listen with empty host throws error
void test_listen_empty_host() {
  TEST("listen with empty host throws error");

  const char* test_conf = "/tmp/test_listen_empty_host.conf";
  std::ofstream file(test_conf);
  file << "server {\n";
  file << "    listen :8080;\n";
  file << "}\n";
  file.close();

  MainConfig config;
  ConfigParser parser(test_conf);
  try {
    parser.parse(config);
    FAIL("should throw on empty host");
  } catch (const std::runtime_error& e) {
    std::string msg(e.what());
    ASSERT_TRUE(msg.find("empty host") != std::string::npos);
  }

  PASS();
}

// Test: listen with empty port throws error
void test_listen_empty_port() {
  TEST("listen with empty port throws error");

  const char* test_conf = "/tmp/test_listen_empty_port.conf";
  std::ofstream file(test_conf);
  file << "server {\n";
  file << "    listen 127.0.0.1:;\n";
  file << "}\n";
  file.close();

  MainConfig config;
  ConfigParser parser(test_conf);
  try {
    parser.parse(config);
    FAIL("should throw on empty port");
  } catch (const std::runtime_error& e) {
    std::string msg(e.what());
    ASSERT_TRUE(msg.find("empty port") != std::string::npos);
  }

  PASS();
}

// Test: error_page without path throws error
void test_error_page_requires_path() {
  TEST("error_page without path throws error");

  const char* test_conf = "/tmp/test_error_page_no_path.conf";
  std::ofstream file(test_conf);
  file << "server {\n";
  file << "    listen 8080;\n";
  file << "    error_page 404;\n";
  file << "}\n";
  file.close();

  MainConfig config;
  ConfigParser parser(test_conf);
  try {
    parser.parse(config);
    FAIL("should throw on missing path");
  } catch (const std::runtime_error& e) {
    std::string msg(e.what());
    ASSERT_TRUE(msg.find("URI/path") != std::string::npos);
  }

  PASS();
}

// Test: listen with trailing characters throws error
void test_listen_trailing_chars() {
  TEST("listen with trailing characters throws error");

  const char* test_conf = "/tmp/test_listen_trailing.conf";
  std::ofstream file(test_conf);
  file << "server {\n";
  file << "    listen 8080abc;\n";
  file << "}\n";
  file.close();

  MainConfig config;
  ConfigParser parser(test_conf);
  try {
    parser.parse(config);
    FAIL("should throw on trailing characters");
  } catch (const std::runtime_error& e) {
    std::string msg(e.what());
    ASSERT_TRUE(msg.find("invalid port number") != std::string::npos);
  }

  PASS();
}

// Test: MainConfig::load() success
void test_mainconfig_load_success() {
  TEST("MainConfig::load() returns true on valid config");

  const char* test_conf = "/tmp/test_load_success.conf";
  std::ofstream file(test_conf);
  file << "server {\n";
  file << "    listen 8080;\n";
  file << "    server_name localhost;\n";
  file << "}\n";
  file.close();

  MainConfig config;
  ASSERT_TRUE(config.load(test_conf));
  ASSERT_EQ(1u, config.servers.size());
  ASSERT_EQ(8080, config.servers[0].listen_port);

  PASS();
}

// Test: MainConfig::load() failure
void test_mainconfig_load_failure() {
  TEST("MainConfig::load() returns false on invalid config");

  const char* test_conf = "/tmp/test_load_failure.conf";
  std::ofstream file(test_conf);
  file << "server {\n";
  file << "    server_name localhost;\n";  // listen missing
  file << "}\n";
  file.close();

  MainConfig config;
  ASSERT_TRUE(!config.load(test_conf));  // should return false
  ASSERT_EQ(0u, config.servers.size());  // no servers loaded

  PASS();
}

// Test: MainConfig::load() file not found
void test_mainconfig_load_file_not_found() {
  TEST("MainConfig::load() returns false when file not found");

  MainConfig config;
  ASSERT_TRUE(!config.load("/tmp/nonexistent_config_file_12345.conf"));

  PASS();
}

// ============================================================================
// Main
// ============================================================================

int main() {
  std::cout << std::endl;
  std::cout << "========================================" << std::endl;
  std::cout << "ConfigParser Tests" << std::endl;
  std::cout << "========================================" << std::endl;
  std::cout << std::endl;

  test_parse_basic_server();
  test_parse_location();
  test_parse_allowed_methods();
  test_parse_error_page();
  test_parse_client_max_body_size();
  test_parse_cgi();
  test_parse_return();
  test_parse_comment();
  test_parse_multiple_servers();
  test_parse_host_port();
  test_server_name_normalization();
  test_duplicate_listen_error();
  test_duplicate_location_error();
  test_error_has_line_number();
  test_return_invalid_status();
  test_server_name_trailing_dot();
  test_error_page_requires_code();
  test_ipv6_not_supported();
  test_duplicate_return_error();
  test_allowed_methods_dedup();
  test_listen_required();
  test_error_page_invalid_code_range();
  test_listen_multiple_colons();
  test_listen_empty_host();
  test_listen_empty_port();
  test_error_page_requires_path();
  test_listen_trailing_chars();
  test_mainconfig_load_success();
  test_mainconfig_load_failure();
  test_mainconfig_load_file_not_found();

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
