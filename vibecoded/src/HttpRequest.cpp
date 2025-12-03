#include "HttpRequest.hpp"

HttpRequest::HttpRequest()
    : method(""),
      uri(""),
      httpVersion(""),
      host(""),
      port(""),
      path(""),
      query("") {}
