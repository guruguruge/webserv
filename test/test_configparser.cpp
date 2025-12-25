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

#define ASSERT_EQ(expected, actual) \
  if ((expected) != (actual))       \
  FAIL(#actual " != " #expected)

#define ASSERT_TRUE(cond) \
  if (!(cond))            \
  FAIL(#cond " is false")

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
