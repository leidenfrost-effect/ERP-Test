#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <crow.h>
#include <google/protobuf/util/json_util.h>

#include "core/error.hpp"
#include "core/inventory_service.hpp"
#include "core/product_service.hpp"
#include "core/person_service.hpp"
#include "infra/mapping/product_mapper.hpp"
#include "infra/mapping/person_mapper.hpp"
#include "infra/sqlite/sqlite_inventory_repository.hpp"
#include "infra/sqlite/sqlite_product_repository.hpp"
#include "infra/sqlite/sqlite_person_repository.hpp"
#include "person.pb.h"

namespace personpb = hybrid::people::v1;

namespace {

struct AuthConfig {
  bool enabled{false};
  bool authorization_enabled{false};
  std::vector<std::string> tokens;
  std::unordered_map<std::string, std::unordered_set<std::string>> token_permissions;
  std::unordered_set<std::string> default_permissions;
};

enum class RuntimeEnvironment {
  Development,
  Production
};

struct Metrics {
  std::atomic<std::uint64_t> requests_total{0};
  std::atomic<std::uint64_t> responses_2xx{0};
  std::atomic<std::uint64_t> responses_4xx{0};
  std::atomic<std::uint64_t> responses_5xx{0};
  std::atomic<std::uint64_t> unauthorized_total{0};
  std::atomic<std::uint64_t> parse_error_total{0};
  std::atomic<std::uint64_t> duration_ms_total{0};
};

Metrics g_metrics;
std::atomic<std::uint64_t> g_request_sequence{0};
std::atomic<std::uint64_t> g_trace_sequence{0};

std::string Trim(const std::string& value) {
  const auto first = value.find_first_not_of(" \t\n\r");
  if (first == std::string::npos) {
    return "";
  }
  const auto last = value.find_last_not_of(" \t\n\r");
  return value.substr(first, last - first + 1);
}

std::vector<std::string> SplitCsv(const std::string& raw) {
  std::vector<std::string> items;
  std::size_t start = 0;

  while (start <= raw.size()) {
    const auto comma = raw.find(',', start);
    const auto token = raw.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
    auto trimmed = Trim(token);
    if (!trimmed.empty()) {
      items.push_back(std::move(trimmed));
    }
    if (comma == std::string::npos) {
      break;
    }
    start = comma + 1;
  }

  return items;
}

std::vector<std::string> SplitBy(const std::string& raw, const char delimiter) {
  std::vector<std::string> items;
  std::size_t start = 0;

  while (start <= raw.size()) {
    const auto idx = raw.find(delimiter, start);
    const auto item = raw.substr(start, idx == std::string::npos ? std::string::npos : idx - start);
    items.push_back(item);
    if (idx == std::string::npos) {
      break;
    }
    start = idx + 1;
  }
  return items;
}

std::unordered_set<std::string> ParsePermissionSet(const std::string& raw) {
  std::unordered_set<std::string> permissions;
  for (const auto& part : SplitBy(raw, '|')) {
    auto trimmed = Trim(part);
    if (!trimmed.empty()) {
      permissions.insert(std::move(trimmed));
    }
  }
  return permissions;
}

bool ParseTokenPermissions(
  const std::string& raw,
  std::unordered_map<std::string, std::unordered_set<std::string>>* token_permissions,
  std::string* error_message) {
  if (token_permissions == nullptr) {
    return false;
  }

  token_permissions->clear();
  for (const auto& entry : SplitCsv(raw)) {
    const auto normalized_entry = Trim(entry);
    if (normalized_entry.empty()) {
      continue;
    }

    const auto eq = normalized_entry.find('=');
    if (eq == std::string::npos) {
      if (error_message != nullptr) {
        *error_message = "Invalid PERSON_API_TOKEN_PERMISSIONS format. Expected token=perm1|perm2.";
      }
      return false;
    }

    auto token = Trim(normalized_entry.substr(0, eq));
    auto permission_raw = Trim(normalized_entry.substr(eq + 1));
    if (token.empty()) {
      if (error_message != nullptr) {
        *error_message = "Token key must not be empty in PERSON_API_TOKEN_PERMISSIONS.";
      }
      return false;
    }

    auto parsed_permissions = ParsePermissionSet(permission_raw);
    if (parsed_permissions.empty()) {
      if (error_message != nullptr) {
        *error_message = "Permission set must not be empty for token '" + token + "'.";
      }
      return false;
    }

    (*token_permissions)[std::move(token)] = std::move(parsed_permissions);
  }

  return true;
}

bool ParseIntegerInRange(const std::string& raw, const int min_value, const int max_value, int* parsed) {
  if (parsed == nullptr) {
    return false;
  }

  try {
    std::size_t consumed = 0;
    const int value = std::stoi(raw, &consumed);
    if (consumed != raw.size() || value < min_value || value > max_value) {
      return false;
    }
    *parsed = value;
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

int HttpStatusFromError(const core::Error& error) {
  switch (error.code) {
    case core::ErrorCode::ValidationFailed:
    case core::ErrorCode::ParseError:
      return 400;
    case core::ErrorCode::Unauthorized:
      return 401;
    case core::ErrorCode::Forbidden:
      return 403;
    case core::ErrorCode::NotFound:
      return 404;
    case core::ErrorCode::RepositoryError:
      return 500;
  }
  return 500;
}

std::string HttpMethodToString(crow::HTTPMethod method) {
  switch (method) {
    case crow::HTTPMethod::Delete:
      return "DELETE";
    case crow::HTTPMethod::Get:
      return "GET";
    case crow::HTTPMethod::Head:
      return "HEAD";
    case crow::HTTPMethod::Post:
      return "POST";
    case crow::HTTPMethod::Put:
      return "PUT";
    case crow::HTTPMethod::Patch:
      return "PATCH";
    case crow::HTTPMethod::Options:
      return "OPTIONS";
    default:
      return "UNKNOWN";
  }
}

std::string EscapeJson(const std::string& value) {
  std::ostringstream out;
  for (const char c : value) {
    switch (c) {
      case '"':
        out << "\\\"";
        break;
      case '\\':
        out << "\\\\";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
        out << c;
        break;
    }
  }
  return out.str();
}

core::Error SanitizeErrorForClient(const core::Error& error) {
  if (error.code == core::ErrorCode::RepositoryError) {
    return core::Error{
      core::ErrorCode::RepositoryError,
      "Internal server error.",
      {}
    };
  }

  core::Error sanitized = error;
  if (sanitized.details.size() > 8) {
    sanitized.details.resize(8);
  }
  for (auto& detail : sanitized.details) {
    if (detail.size() > 256) {
      detail = detail.substr(0, 256);
    }
  }
  return sanitized;
}

void RecordMetrics(const int status, const std::int64_t duration_ms, const core::Error* error = nullptr) {
  g_metrics.requests_total.fetch_add(1, std::memory_order_relaxed);
  g_metrics.duration_ms_total.fetch_add(
    static_cast<std::uint64_t>(std::max<std::int64_t>(duration_ms, 0)), std::memory_order_relaxed);

  if (status >= 200 && status < 300) {
    g_metrics.responses_2xx.fetch_add(1, std::memory_order_relaxed);
  } else if (status >= 400 && status < 500) {
    g_metrics.responses_4xx.fetch_add(1, std::memory_order_relaxed);
  } else if (status >= 500) {
    g_metrics.responses_5xx.fetch_add(1, std::memory_order_relaxed);
  }

  if (error != nullptr) {
    if (error->code == core::ErrorCode::Unauthorized) {
      g_metrics.unauthorized_total.fetch_add(1, std::memory_order_relaxed);
    }
    if (error->code == core::ErrorCode::ParseError) {
      g_metrics.parse_error_total.fetch_add(1, std::memory_order_relaxed);
    }
  }
}

void LogAccess(
  const crow::request& request,
  const int status,
  const std::int64_t duration_ms,
  const std::string& request_id,
  const std::string& correlation_id,
  const std::string& trace_id) {
  const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();

  std::cout << "{\"ts_ms\":" << now_ms << ",\"level\":\"INFO\",\"event\":\"http_access\""
            << ",\"request_id\":\"" << EscapeJson(request_id) << "\""
            << ",\"correlation_id\":\"" << EscapeJson(correlation_id) << "\""
            << ",\"trace_id\":\"" << EscapeJson(trace_id) << "\""
            << ",\"method\":\"" << HttpMethodToString(request.method) << "\""
            << ",\"path\":\"" << EscapeJson(request.url) << "\""
            << ",\"status\":" << status
            << ",\"duration_ms\":" << duration_ms << "}" << std::endl;
}

void ApplySecurityHeaders(crow::response* response) {
  if (response == nullptr) {
    return;
  }

  response->set_header("X-Content-Type-Options", "nosniff");
  response->set_header("X-Frame-Options", "DENY");
  response->set_header("Referrer-Policy", "no-referrer");
  response->set_header("Cache-Control", "no-store");
}

std::string ResolveRequestId(const crow::request& request) {
  const std::string incoming = request.get_header_value("X-Request-Id");
  if (!incoming.empty()) {
    return incoming;
  }

  const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();
  const auto seq = g_request_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
  return std::to_string(now_ms) + "-" + std::to_string(seq);
}

std::string ToLower(const std::string& value) {
  std::string lowered = value;
  std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return lowered;
}

std::string ToHexFixed(const std::uint64_t value, const std::size_t width) {
  std::ostringstream out;
  out << std::hex << std::nouppercase << std::setfill('0') << std::setw(static_cast<int>(width)) << value;
  auto rendered = out.str();
  if (rendered.size() > width) {
    rendered = rendered.substr(rendered.size() - width);
  }
  return rendered;
}

bool IsHexChar(const char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

bool IsHexRange(const std::string& value, const std::size_t start, const std::size_t length) {
  if (start + length > value.size()) {
    return false;
  }

  for (std::size_t i = start; i < start + length; ++i) {
    if (!IsHexChar(value[i])) {
      return false;
    }
  }
  return true;
}

bool IsZeroHex(const std::string& value) {
  return std::all_of(value.begin(), value.end(), [](const char c) { return c == '0'; });
}

bool IsValidTraceparent(const std::string& traceparent) {
  // W3C traceparent: version(2)-trace_id(32)-span_id(16)-flags(2)
  if (traceparent.size() != 55) {
    return false;
  }
  if (traceparent[2] != '-' || traceparent[35] != '-' || traceparent[52] != '-') {
    return false;
  }
  if (!IsHexRange(traceparent, 0, 2) || !IsHexRange(traceparent, 3, 32) ||
      !IsHexRange(traceparent, 36, 16) || !IsHexRange(traceparent, 53, 2)) {
    return false;
  }
  return true;
}

std::string ExtractTraceId(const std::string& traceparent) {
  if (!IsValidTraceparent(traceparent)) {
    return "";
  }
  return ToLower(traceparent.substr(3, 32));
}

std::string ResolveCorrelationId(const crow::request& request, const std::string& request_id) {
  auto incoming = Trim(request.get_header_value("X-Correlation-Id"));
  if (incoming.empty()) {
    return request_id;
  }
  if (incoming.size() > 128) {
    incoming = incoming.substr(0, 128);
  }
  return incoming;
}

std::string GenerateTraceparent(const std::string& seed) {
  const auto seq = g_trace_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
  const auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();

  const auto stable_seed = seed + ":" + std::to_string(now_ns) + ":" + std::to_string(seq);
  const auto hash_a = static_cast<std::uint64_t>(std::hash<std::string>{}(stable_seed));
  const auto hash_b = static_cast<std::uint64_t>(std::hash<std::string>{}(stable_seed + ":trace"));
  const auto span_hash = static_cast<std::uint64_t>(std::hash<std::string>{}(stable_seed + ":span"));

  std::string trace_id = ToHexFixed(hash_a, 16) + ToHexFixed(hash_b, 16);
  std::string span_id = ToHexFixed(span_hash, 16);

  if (IsZeroHex(trace_id)) {
    trace_id.back() = '1';
  }
  if (IsZeroHex(span_id)) {
    span_id.back() = '1';
  }

  return "00-" + ToLower(trace_id) + "-" + ToLower(span_id) + "-01";
}

std::string ResolveTraceparent(
  const crow::request& request,
  const std::string& request_id,
  const std::string& correlation_id) {
  const auto incoming = Trim(request.get_header_value("traceparent"));
  if (IsValidTraceparent(incoming)) {
    return ToLower(incoming);
  }
  return GenerateTraceparent(request_id + ":" + correlation_id + ":" + request.url);
}

std::string ResolveAuditActor(const crow::request& request) {
  const std::string authorization = request.get_header_value("Authorization");
  const std::string bearer_prefix = "Bearer ";
  if (authorization.rfind(bearer_prefix, 0) != 0) {
    return "anonymous";
  }

  const auto token = Trim(authorization.substr(bearer_prefix.size()));
  if (token.empty()) {
    return "anonymous";
  }
  return "token:" + ToHexFixed(static_cast<std::uint64_t>(std::hash<std::string>{}(token)), 16);
}

bool IsWriteMethod(const crow::HTTPMethod method) {
  return method == crow::HTTPMethod::Post || method == crow::HTTPMethod::Put ||
         method == crow::HTTPMethod::Patch || method == crow::HTTPMethod::Delete;
}

std::string AuditAction(const crow::HTTPMethod method) {
  switch (method) {
    case crow::HTTPMethod::Post:
      return "CREATE";
    case crow::HTTPMethod::Put:
    case crow::HTTPMethod::Patch:
      return "UPDATE";
    case crow::HTTPMethod::Delete:
      return "DELETE";
    default:
      return "WRITE";
  }
}

std::vector<std::string> SplitPathSegments(const std::string& raw_url) {
  std::string path = raw_url;
  if (const auto query_start = path.find('?'); query_start != std::string::npos) {
    path = path.substr(0, query_start);
  }

  std::vector<std::string> segments;
  std::string current;
  for (const char c : path) {
    if (c == '/') {
      if (!current.empty()) {
        segments.push_back(current);
        current.clear();
      }
      continue;
    }
    current.push_back(c);
  }
  if (!current.empty()) {
    segments.push_back(std::move(current));
  }
  return segments;
}

std::pair<std::string, std::string> ResolveAuditResource(const std::string& raw_url) {
  auto segments = SplitPathSegments(raw_url);
  if (segments.empty()) {
    return { "root", "" };
  }

  std::size_t idx = 0;
  if (segments[0] == "pb" && segments.size() > 1) {
    idx = 1;
  }

  const std::string resource_type = segments[idx];
  std::string resource_id;
  if (segments.size() > idx + 1 && segments[idx + 1] != "movements") {
    resource_id = segments[idx + 1];
  }
  return { resource_type, resource_id };
}

void LogAuditEvent(
  const crow::request& request,
  const int status,
  const std::string& request_id,
  const std::string& correlation_id,
  const std::string& trace_id) {
  if (!IsWriteMethod(request.method) || status < 200 || status >= 300) {
    return;
  }

  const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();
  const auto resource = ResolveAuditResource(request.url);

  std::cout << "{\"ts_ms\":" << now_ms << ",\"level\":\"INFO\",\"event\":\"audit\""
            << ",\"request_id\":\"" << EscapeJson(request_id) << "\""
            << ",\"correlation_id\":\"" << EscapeJson(correlation_id) << "\""
            << ",\"trace_id\":\"" << EscapeJson(trace_id) << "\""
            << ",\"action\":\"" << EscapeJson(AuditAction(request.method)) << "\""
            << ",\"resource_type\":\"" << EscapeJson(resource.first) << "\""
            << ",\"resource_id\":\"" << EscapeJson(resource.second) << "\""
            << ",\"actor\":\"" << EscapeJson(ResolveAuditActor(request)) << "\""
            << ",\"status\":" << status << "}" << std::endl;
}

crow::response FinalizeResponse(
  const crow::request& request,
  crow::response response,
  const std::chrono::steady_clock::time_point started,
  const std::string& request_id,
  const core::Error* error = nullptr) {
  ApplySecurityHeaders(&response);

  const auto correlation_id = ResolveCorrelationId(request, request_id);
  const auto traceparent = ResolveTraceparent(request, request_id, correlation_id);
  const auto trace_id = ExtractTraceId(traceparent);

  response.set_header("X-Request-Id", request_id);
  response.set_header("X-Correlation-Id", correlation_id);
  response.set_header("traceparent", traceparent);

  const auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - started)
                             .count();

  RecordMetrics(response.code, duration_ms, error);
  LogAccess(request, response.code, duration_ms, request_id, correlation_id, trace_id);
  LogAuditEvent(request, response.code, request_id, correlation_id, trace_id);
  return response;
}

template <typename TMessage>
crow::response JsonResponse(const TMessage& message, const int status_code) {
  std::string json;
  google::protobuf::util::JsonPrintOptions options;
  options.add_whitespace = false;
  options.preserve_proto_field_names = false;
  const auto status = google::protobuf::util::MessageToJsonString(message, &json, options);
  if (!status.ok()) {
    crow::response fallback(500);
    fallback.set_header("Content-Type", "application/json");
    fallback.body = R"({"error":"Failed to serialize protobuf response."})";
    return fallback;
  }

  crow::response response(status_code);
  response.set_header("Content-Type", "application/json");
  response.body = std::move(json);
  return response;
}

template <typename TMessage>
crow::response ProtobufResponse(const TMessage& message, const int status_code) {
  std::string payload;
  if (!message.SerializeToString(&payload)) {
    crow::response fallback(500);
    fallback.set_header("Content-Type", "application/json");
    fallback.body = R"({"error":"Failed to serialize protobuf response."})";
    return fallback;
  }

  crow::response response(status_code);
  response.set_header("Content-Type", "application/x-protobuf");
  response.body = std::move(payload);
  return response;
}

template <typename TRequest>
core::Result<TRequest> ParseJsonBody(const crow::request& request) {
  TRequest parsed;
  google::protobuf::util::JsonParseOptions options;
  options.ignore_unknown_fields = false;

  const auto status = google::protobuf::util::JsonStringToMessage(request.body, &parsed, options);
  if (!status.ok()) {
    return core::Result<TRequest>::Failure(core::Error{
      core::ErrorCode::ParseError,
      "Invalid JSON payload.",
      { status.ToString() }
    });
  }

  return core::Result<TRequest>::Success(std::move(parsed));
}

template <typename TRequest>
core::Result<TRequest> ParseProtobufBody(const crow::request& request) {
  TRequest parsed;
  if (!parsed.ParseFromString(request.body)) {
    return core::Result<TRequest>::Failure(core::Error{
      core::ErrorCode::ParseError,
      "Invalid protobuf payload.",
      {}
    });
  }
  return core::Result<TRequest>::Success(std::move(parsed));
}

template <typename TResponse>
crow::response JsonErrorResponse(const core::Error& error) {
  TResponse response;
  const auto sanitized = SanitizeErrorForClient(error);
  *response.mutable_error() = infra::mapping::ToProtoApiError(sanitized);
  return JsonResponse(response, HttpStatusFromError(sanitized));
}

template <typename TResponse>
crow::response ProtobufErrorResponse(const core::Error& error) {
  TResponse response;
  const auto sanitized = SanitizeErrorForClient(error);
  *response.mutable_error() = infra::mapping::ToProtoApiError(sanitized);
  return ProtobufResponse(response, HttpStatusFromError(sanitized));
}

core::Error UnauthorizedError() {
  return core::Error{
    core::ErrorCode::Unauthorized,
    "Unauthorized.",
    { "Provide Authorization: Bearer <token>." }
  };
}

core::Error ForbiddenError(const std::string& permission) {
  core::Error error{
    core::ErrorCode::Forbidden,
    "Forbidden.",
    {}
  };
  if (!permission.empty()) {
    error.details.push_back("Missing permission: " + permission);
  }
  return error;
}

std::optional<std::string> ExtractBearerToken(const crow::request& request) {
  const std::string authorization = request.get_header_value("Authorization");
  const std::string bearer_prefix = "Bearer ";
  if (authorization.rfind(bearer_prefix, 0) != 0) {
    return std::nullopt;
  }

  auto token = Trim(authorization.substr(bearer_prefix.size()));
  if (token.empty()) {
    return std::nullopt;
  }
  return token;
}

bool IsAuthorized(const crow::request& request, const AuthConfig& config) {
  if (!config.enabled) {
    return true;
  }

  const auto token = ExtractBearerToken(request);
  if (!token.has_value()) {
    return false;
  }

  return std::find(config.tokens.begin(), config.tokens.end(), token.value()) != config.tokens.end();
}

bool HasPermission(const AuthConfig& config, const std::string& token, const std::string& permission) {
  if (!config.authorization_enabled) {
    return true;
  }

  const auto token_it = config.token_permissions.find(token);
  const auto& permissions = token_it != config.token_permissions.end() ? token_it->second : config.default_permissions;
  if (permissions.empty()) {
    return false;
  }
  if (permissions.find("*") != permissions.end()) {
    return true;
  }
  return permissions.find(permission) != permissions.end();
}

core::Result<bool> EnsureAuthorized(
  const crow::request& request,
  const AuthConfig& config,
  const std::string& permission) {
  if (!IsAuthorized(request, config)) {
    return core::Result<bool>::Failure(UnauthorizedError());
  }

  if (!HasPermission(config, ExtractBearerToken(request).value_or(""), permission)) {
    return core::Result<bool>::Failure(ForbiddenError(permission));
  }

  return core::Result<bool>::Success(true);
}

core::Result<bool> EnsureAuthenticated(const crow::request& request, const AuthConfig& config) {
  if (!IsAuthorized(request, config)) {
    return core::Result<bool>::Failure(UnauthorizedError());
  }
  return core::Result<bool>::Success(true);
}

core::Result<int> ParseIntegerQueryParam(const crow::request& request, const char* name, const int fallback_value) {
  const char* raw = request.url_params.get(name);
  if (raw == nullptr) {
    return core::Result<int>::Success(fallback_value);
  }

  std::string raw_value = raw;
  try {
    std::size_t parsed_chars = 0;
    const int parsed = std::stoi(raw_value, &parsed_chars);
    if (parsed_chars != raw_value.size()) {
      return core::Result<int>::Failure(core::Error{
        core::ErrorCode::ParseError,
        "Invalid query parameter.",
        { std::string(name) + " must be an integer." }
      });
    }
    return core::Result<int>::Success(parsed);
  } catch (const std::exception&) {
    return core::Result<int>::Failure(core::Error{
      core::ErrorCode::ParseError,
      "Invalid query parameter.",
      { std::string(name) + " must be an integer." }
    });
  }
}

core::Result<core::ListPersonsQuery> ParseListQuery(const crow::request& request) {
  auto page_result = ParseIntegerQueryParam(request, "page", 1);
  if (!page_result.ok()) {
    return core::Result<core::ListPersonsQuery>::Failure(page_result.error());
  }

  auto page_size_result = ParseIntegerQueryParam(request, "pageSize", 20);
  if (!page_size_result.ok()) {
    return core::Result<core::ListPersonsQuery>::Failure(page_size_result.error());
  }

  if (request.url_params.get("page_size") != nullptr) {
    page_size_result = ParseIntegerQueryParam(request, "page_size", page_size_result.value());
    if (!page_size_result.ok()) {
      return core::Result<core::ListPersonsQuery>::Failure(page_size_result.error());
    }
  }

  core::ListPersonsQuery query;
  query.page = page_result.value();
  query.page_size = page_size_result.value();
  if (const char* search = request.url_params.get("q"); search != nullptr) {
    query.search = search;
  }
  return core::Result<core::ListPersonsQuery>::Success(std::move(query));
}

core::Result<core::ListProductsQuery> ParseListProductsQuery(const crow::request& request) {
  auto page_result = ParseIntegerQueryParam(request, "page", 1);
  if (!page_result.ok()) {
    return core::Result<core::ListProductsQuery>::Failure(page_result.error());
  }

  auto page_size_result = ParseIntegerQueryParam(request, "pageSize", 20);
  if (!page_size_result.ok()) {
    return core::Result<core::ListProductsQuery>::Failure(page_size_result.error());
  }

  if (request.url_params.get("page_size") != nullptr) {
    page_size_result = ParseIntegerQueryParam(request, "page_size", page_size_result.value());
    if (!page_size_result.ok()) {
      return core::Result<core::ListProductsQuery>::Failure(page_size_result.error());
    }
  }

  core::ListProductsQuery query;
  query.page = page_result.value();
  query.page_size = page_size_result.value();
  if (const char* search = request.url_params.get("q"); search != nullptr) {
    query.search = search;
  }
  return core::Result<core::ListProductsQuery>::Success(std::move(query));
}

struct StockBalanceQuery {
  std::int64_t product_id{0};
  std::string warehouse_code;
};

core::Result<StockBalanceQuery> ParseStockBalanceQuery(const crow::request& request) {
  auto product_id_result = ParseIntegerQueryParam(request, "productId", -1);
  if (!product_id_result.ok()) {
    return core::Result<StockBalanceQuery>::Failure(product_id_result.error());
  }

  if (request.url_params.get("product_id") != nullptr) {
    product_id_result = ParseIntegerQueryParam(request, "product_id", product_id_result.value());
    if (!product_id_result.ok()) {
      return core::Result<StockBalanceQuery>::Failure(product_id_result.error());
    }
  }

  if (product_id_result.value() <= 0) {
    return core::Result<StockBalanceQuery>::Failure(core::Error{
      core::ErrorCode::ValidationFailed,
      "Invalid query parameter.",
      { "productId must be greater than zero." }
    });
  }

  std::string warehouse_code = "MAIN";
  if (const char* raw = request.url_params.get("warehouse"); raw != nullptr) {
    warehouse_code = Trim(raw);
  }
  if (const char* raw = request.url_params.get("warehouseCode"); raw != nullptr) {
    warehouse_code = Trim(raw);
  }
  if (const char* raw = request.url_params.get("warehouse_code"); raw != nullptr) {
    warehouse_code = Trim(raw);
  }

  if (warehouse_code.empty()) {
    return core::Result<StockBalanceQuery>::Failure(core::Error{
      core::ErrorCode::ValidationFailed,
      "Invalid query parameter.",
      { "warehouseCode must not be empty." }
    });
  }

  StockBalanceQuery query;
  query.product_id = product_id_result.value();
  query.warehouse_code = std::move(warehouse_code);
  return core::Result<StockBalanceQuery>::Success(std::move(query));
}

std::string BuildPrometheusMetrics() {
  const auto requests_total = g_metrics.requests_total.load(std::memory_order_relaxed);
  const auto responses_2xx = g_metrics.responses_2xx.load(std::memory_order_relaxed);
  const auto responses_4xx = g_metrics.responses_4xx.load(std::memory_order_relaxed);
  const auto responses_5xx = g_metrics.responses_5xx.load(std::memory_order_relaxed);
  const auto unauthorized_total = g_metrics.unauthorized_total.load(std::memory_order_relaxed);
  const auto parse_error_total = g_metrics.parse_error_total.load(std::memory_order_relaxed);
  const auto duration_ms_total = g_metrics.duration_ms_total.load(std::memory_order_relaxed);

  std::ostringstream out;
  out << "# HELP person_api_http_requests_total Total HTTP requests.\n";
  out << "# TYPE person_api_http_requests_total counter\n";
  out << "person_api_http_requests_total " << requests_total << '\n';

  out << "# HELP person_api_http_responses_total HTTP response classes.\n";
  out << "# TYPE person_api_http_responses_total counter\n";
  out << "person_api_http_responses_total{class=\"2xx\"} " << responses_2xx << '\n';
  out << "person_api_http_responses_total{class=\"4xx\"} " << responses_4xx << '\n';
  out << "person_api_http_responses_total{class=\"5xx\"} " << responses_5xx << '\n';

  out << "# HELP person_api_auth_unauthorized_total Unauthorized responses.\n";
  out << "# TYPE person_api_auth_unauthorized_total counter\n";
  out << "person_api_auth_unauthorized_total " << unauthorized_total << '\n';

  out << "# HELP person_api_parse_error_total Parse errors returned by API.\n";
  out << "# TYPE person_api_parse_error_total counter\n";
  out << "person_api_parse_error_total " << parse_error_total << '\n';

  out << "# HELP person_api_http_duration_ms_total Total request duration in milliseconds.\n";
  out << "# TYPE person_api_http_duration_ms_total counter\n";
  out << "person_api_http_duration_ms_total " << duration_ms_total << '\n';

  return out.str();
}

unsigned int DetermineDefaultConcurrency() {
  const unsigned int hw_threads = std::thread::hardware_concurrency();
  if (hw_threads <= 1) {
    return 2;
  }
  return hw_threads;
}

bool TryGetEnvString(const char* name, std::string* value) {
  if (name == nullptr || value == nullptr) {
    return false;
  }
  if (const char* raw = std::getenv(name); raw != nullptr) {
    *value = raw;
    return true;
  }
  return false;
}

RuntimeEnvironment ParseRuntimeEnvironment(const std::string& value) {
  std::string normalized = Trim(value);
  std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });

  if (normalized == "prod" || normalized == "production") {
    return RuntimeEnvironment::Production;
  }
  return RuntimeEnvironment::Development;
}

std::string RuntimeEnvironmentToString(const RuntimeEnvironment env) {
  if (env == RuntimeEnvironment::Production) {
    return "production";
  }
  return "development";
}

std::string AuthStatusValue(const AuthConfig& auth) {
  if (auth.enabled) {
    if (auth.authorization_enabled) {
      return "token_and_rbac_required";
    }
    return "token_required";
  }
  return "no_auth";
}

}  // namespace

int main(int argc, char* argv[]) {
  std::string database_path = "persons.db";
  std::uint16_t port = 18080;
  unsigned int concurrency = DetermineDefaultConcurrency();

  std::string env_value;
  if (argc <= 1 && TryGetEnvString("PERSON_API_DB_PATH", &env_value) && !Trim(env_value).empty()) {
    database_path = Trim(env_value);
  }

  if (argc > 1) {
    database_path = argv[1];
  }

  if (argc <= 2 && TryGetEnvString("PERSON_API_PORT", &env_value) && !Trim(env_value).empty()) {
    int parsed_port = 0;
    if (!ParseIntegerInRange(Trim(env_value), 1, 65535, &parsed_port)) {
      std::cerr << "Invalid PERSON_API_PORT value.\n";
      return 1;
    }
    port = static_cast<std::uint16_t>(parsed_port);
  }

  if (argc > 2) {
    int parsed_port = 0;
    if (!ParseIntegerInRange(argv[2], 1, 65535, &parsed_port)) {
      std::cerr << "Port must be between 1 and 65535.\n";
      return 1;
    }
    port = static_cast<std::uint16_t>(parsed_port);
  }

  if (argc <= 3 && TryGetEnvString("PERSON_API_CONCURRENCY", &env_value) && !Trim(env_value).empty()) {
    int parsed_concurrency = 0;
    if (!ParseIntegerInRange(Trim(env_value), 1, 64, &parsed_concurrency)) {
      std::cerr << "Invalid PERSON_API_CONCURRENCY value.\n";
      return 1;
    }
    concurrency = static_cast<unsigned int>(parsed_concurrency);
  }

  if (argc > 3) {
    int parsed_concurrency = 0;
    if (!ParseIntegerInRange(argv[3], 1, 64, &parsed_concurrency)) {
      std::cerr << "Concurrency must be between 1 and 64.\n";
      return 1;
    }
    concurrency = static_cast<unsigned int>(parsed_concurrency);
  }

  AuthConfig auth;
  if (TryGetEnvString("PERSON_API_TOKENS", &env_value)) {
    auth.tokens = SplitCsv(env_value);
  } else if (TryGetEnvString("PERSON_API_TOKEN", &env_value) && !Trim(env_value).empty()) {
    auth.tokens = { Trim(env_value) };
  }
  auth.enabled = !auth.tokens.empty();

  if (TryGetEnvString("PERSON_API_TOKEN_PERMISSIONS", &env_value) && !Trim(env_value).empty()) {
    std::string parse_error;
    if (!ParseTokenPermissions(env_value, &auth.token_permissions, &parse_error)) {
      std::cerr << parse_error << '\n';
      return 1;
    }
  }
  if (TryGetEnvString("PERSON_API_DEFAULT_PERMISSIONS", &env_value) && !Trim(env_value).empty()) {
    auth.default_permissions = ParsePermissionSet(env_value);
  }
  auth.authorization_enabled = !auth.token_permissions.empty() || !auth.default_permissions.empty();

  RuntimeEnvironment runtime_environment = RuntimeEnvironment::Development;
  if (TryGetEnvString("PERSON_API_ENV", &env_value) && !Trim(env_value).empty()) {
    runtime_environment = ParseRuntimeEnvironment(env_value);
  }
  if (runtime_environment == RuntimeEnvironment::Production && !auth.enabled) {
    std::cerr << "Production mode requires PERSON_API_TOKEN or PERSON_API_TOKENS.\n";
    return 1;
  }
  if (runtime_environment == RuntimeEnvironment::Production && auth.enabled && !auth.authorization_enabled) {
    std::cerr
      << "Production mode with token auth requires PERSON_API_TOKEN_PERMISSIONS or PERSON_API_DEFAULT_PERMISSIONS.\n";
    return 1;
  }

  infra::sqlite::SQLitePersonRepository repository(database_path);
  auto migration_result = repository.Migrate();
  if (!migration_result.ok()) {
    std::cerr << "Migration failed: " << migration_result.error().message << '\n';
    if (!migration_result.error().details.empty()) {
      std::cerr << "Detail: " << migration_result.error().details.front() << '\n';
    }
    return 1;
  }

  infra::sqlite::SQLiteProductRepository product_repository(database_path);
  auto product_migration_result = product_repository.Migrate();
  if (!product_migration_result.ok()) {
    std::cerr << "Migration failed: " << product_migration_result.error().message << '\n';
    if (!product_migration_result.error().details.empty()) {
      std::cerr << "Detail: " << product_migration_result.error().details.front() << '\n';
    }
    return 1;
  }

  infra::sqlite::SQLiteInventoryRepository inventory_repository(database_path);
  auto inventory_migration_result = inventory_repository.Migrate();
  if (!inventory_migration_result.ok()) {
    std::cerr << "Migration failed: " << inventory_migration_result.error().message << '\n';
    if (!inventory_migration_result.error().details.empty()) {
      std::cerr << "Detail: " << inventory_migration_result.error().details.front() << '\n';
    }
    return 1;
  }

  core::PersonService service(repository);
  core::ProductService product_service(product_repository);
  core::InventoryService inventory_service(inventory_repository, product_repository);
  crow::SimpleApp app;

  CROW_ROUTE(app, "/health").methods(crow::HTTPMethod::Get)([](const crow::request& request) {
    const auto started = std::chrono::steady_clock::now();
    const auto request_id = ResolveRequestId(request);

    personpb::HealthResponse response;
    response.set_status("ok");
    return FinalizeResponse(request, JsonResponse(response, 200), started, request_id);
  });

  CROW_ROUTE(app, "/ready").methods(crow::HTTPMethod::Get)([&service](const crow::request& request) {
    const auto started = std::chrono::steady_clock::now();
    const auto request_id = ResolveRequestId(request);

    auto ready_check = service.ListPersons(1, 1);
    personpb::HealthResponse response;
    if (!ready_check.ok()) {
      response.set_status("not_ready");
      return FinalizeResponse(request, JsonResponse(response, 503), started, request_id, &ready_check.error());
    }

    response.set_status("ready");
    return FinalizeResponse(request, JsonResponse(response, 200), started, request_id);
  });

  CROW_ROUTE(app, "/auth/status").methods(crow::HTTPMethod::Get)(
    [&auth](const crow::request& request) {
      const auto started = std::chrono::steady_clock::now();
      const auto request_id = ResolveRequestId(request);

      personpb::HealthResponse response;
      response.set_status(AuthStatusValue(auth));
      return FinalizeResponse(request, JsonResponse(response, 200), started, request_id);
    });

  CROW_ROUTE(app, "/auth/verify").methods(crow::HTTPMethod::Get)(
    [&auth](const crow::request& request) {
      const auto started = std::chrono::steady_clock::now();
      const auto request_id = ResolveRequestId(request);

      auto auth_result = EnsureAuthenticated(request, auth);
      if (!auth_result.ok()) {
        const auto error = auth_result.error();
        return FinalizeResponse(
          request, JsonErrorResponse<personpb::HealthResponse>(error), started, request_id, &error);
      }

      personpb::HealthResponse response;
      response.set_status("authorized");
      return FinalizeResponse(request, JsonResponse(response, 200), started, request_id);
    });

  CROW_ROUTE(app, "/metrics").methods(crow::HTTPMethod::Get)([&auth](const crow::request& request) {
    const auto started = std::chrono::steady_clock::now();
    const auto request_id = ResolveRequestId(request);

    auto auth_result = EnsureAuthorized(request, auth, "metrics.read");
    if (!auth_result.ok()) {
      const auto error = auth_result.error();
      return FinalizeResponse(
        request, JsonErrorResponse<personpb::HealthResponse>(error), started, request_id, &error);
    }

    crow::response response(200);
    response.set_header("Content-Type", "text/plain; version=0.0.4");
    response.body = BuildPrometheusMetrics();
    return FinalizeResponse(request, std::move(response), started, request_id);
  });

  CROW_ROUTE(app, "/persons").methods(crow::HTTPMethod::Get)([&service, &auth](const crow::request& request) {
    const auto started = std::chrono::steady_clock::now();
    const auto request_id = ResolveRequestId(request);

    auto auth_result = EnsureAuthorized(request, auth, "persons.read");
    if (!auth_result.ok()) {
      const auto error = auth_result.error();
      return FinalizeResponse(
        request, JsonErrorResponse<personpb::ListPersonsResponse>(error), started, request_id, &error);
    }

    auto query_result = ParseListQuery(request);
    if (!query_result.ok()) {
      const auto error = query_result.error();
      return FinalizeResponse(
        request, JsonErrorResponse<personpb::ListPersonsResponse>(error), started, request_id, &error);
    }

    const auto result = service.ListPersons(
      query_result.value().page, query_result.value().page_size, query_result.value().search);
    if (!result.ok()) {
      const auto error = result.error();
      return FinalizeResponse(
        request, JsonErrorResponse<personpb::ListPersonsResponse>(error), started, request_id, &error);
    }

    personpb::ListPersonsResponse response;
    for (const auto& person : result.value().persons) {
      *response.add_persons() = infra::mapping::ToProtoPerson(person);
    }
    response.set_page(result.value().page);
    response.set_page_size(result.value().page_size);
    response.set_total(result.value().total);
    response.set_query(result.value().search);
    return FinalizeResponse(request, JsonResponse(response, 200), started, request_id);
  });

  CROW_ROUTE(app, "/persons/<int>").methods(crow::HTTPMethod::Get)([&service, &auth](
                                                                       const crow::request& request,
                                                                       const int id) {
    const auto started = std::chrono::steady_clock::now();
    const auto request_id = ResolveRequestId(request);

    auto auth_result = EnsureAuthorized(request, auth, "persons.read");
    if (!auth_result.ok()) {
      const auto error = auth_result.error();
      return FinalizeResponse(
        request, JsonErrorResponse<personpb::GetPersonResponse>(error), started, request_id, &error);
    }

    const auto result = service.GetPersonById(static_cast<std::int64_t>(id));
    if (!result.ok()) {
      const auto error = result.error();
      return FinalizeResponse(
        request, JsonErrorResponse<personpb::GetPersonResponse>(error), started, request_id, &error);
    }

    personpb::GetPersonResponse response;
    *response.mutable_person() = infra::mapping::ToProtoPerson(result.value());
    return FinalizeResponse(request, JsonResponse(response, 200), started, request_id);
  });

  CROW_ROUTE(app, "/persons").methods(crow::HTTPMethod::Post)([&service, &auth](const crow::request& request) {
    const auto started = std::chrono::steady_clock::now();
    const auto request_id = ResolveRequestId(request);

    auto auth_result = EnsureAuthorized(request, auth, "persons.write");
    if (!auth_result.ok()) {
      const auto error = auth_result.error();
      return FinalizeResponse(
        request, JsonErrorResponse<personpb::CreatePersonResponse>(error), started, request_id, &error);
    }

    auto parse_result = ParseJsonBody<personpb::CreatePersonRequest>(request);
    if (!parse_result.ok()) {
      const auto error = parse_result.error();
      return FinalizeResponse(
        request, JsonErrorResponse<personpb::CreatePersonResponse>(error), started, request_id, &error);
    }

    const auto person = infra::mapping::FromProtoPersonInput(parse_result.value().person());
    auto create_result = service.CreatePerson(person);
    if (!create_result.ok()) {
      const auto error = create_result.error();
      return FinalizeResponse(
        request, JsonErrorResponse<personpb::CreatePersonResponse>(error), started, request_id, &error);
    }

    personpb::CreatePersonResponse response;
    *response.mutable_person() = infra::mapping::ToProtoPerson(create_result.value());
    return FinalizeResponse(request, JsonResponse(response, 201), started, request_id);
  });

  CROW_ROUTE(app, "/persons/<int>").methods(crow::HTTPMethod::Put)(
    [&service, &auth](const crow::request& request, const int id) {
      const auto started = std::chrono::steady_clock::now();
      const auto request_id = ResolveRequestId(request);

      auto auth_result = EnsureAuthorized(request, auth, "persons.write");
      if (!auth_result.ok()) {
        const auto error = auth_result.error();
        return FinalizeResponse(
          request, JsonErrorResponse<personpb::UpdatePersonResponse>(error), started, request_id, &error);
      }

      auto parse_result = ParseJsonBody<personpb::UpdatePersonRequest>(request);
      if (!parse_result.ok()) {
        const auto error = parse_result.error();
        return FinalizeResponse(
          request, JsonErrorResponse<personpb::UpdatePersonResponse>(error), started, request_id, &error);
      }

      const auto person = infra::mapping::FromProtoPersonInput(parse_result.value().person());
      auto update_result = service.UpdatePerson(static_cast<std::int64_t>(id), person);
      if (!update_result.ok()) {
        const auto error = update_result.error();
        return FinalizeResponse(
          request, JsonErrorResponse<personpb::UpdatePersonResponse>(error), started, request_id, &error);
      }

      personpb::UpdatePersonResponse response;
      *response.mutable_person() = infra::mapping::ToProtoPerson(update_result.value());
      return FinalizeResponse(request, JsonResponse(response, 200), started, request_id);
    });

  CROW_ROUTE(app, "/persons/<int>").methods(crow::HTTPMethod::Delete)([&service, &auth](
                                                                            const crow::request& request,
                                                                            const int id) {
    const auto started = std::chrono::steady_clock::now();
    const auto request_id = ResolveRequestId(request);

    auto auth_result = EnsureAuthorized(request, auth, "persons.write");
    if (!auth_result.ok()) {
      const auto error = auth_result.error();
      return FinalizeResponse(
        request, JsonErrorResponse<personpb::DeletePersonResponse>(error), started, request_id, &error);
    }

    auto delete_result = service.DeletePerson(static_cast<std::int64_t>(id));
    if (!delete_result.ok()) {
      const auto error = delete_result.error();
      return FinalizeResponse(
        request, JsonErrorResponse<personpb::DeletePersonResponse>(error), started, request_id, &error);
    }

    personpb::DeletePersonResponse response;
    response.set_deleted(delete_result.value());
    return FinalizeResponse(request, JsonResponse(response, 200), started, request_id);
  });

  CROW_ROUTE(app, "/products").methods(crow::HTTPMethod::Get)([&product_service, &auth](
                                                                 const crow::request& request) {
    const auto started = std::chrono::steady_clock::now();
    const auto request_id = ResolveRequestId(request);

    auto auth_result = EnsureAuthorized(request, auth, "products.read");
    if (!auth_result.ok()) {
      const auto error = auth_result.error();
      return FinalizeResponse(
        request, JsonErrorResponse<personpb::ListProductsResponse>(error), started, request_id, &error);
    }

    auto query_result = ParseListProductsQuery(request);
    if (!query_result.ok()) {
      const auto error = query_result.error();
      return FinalizeResponse(
        request, JsonErrorResponse<personpb::ListProductsResponse>(error), started, request_id, &error);
    }

    const auto result = product_service.ListProducts(
      query_result.value().page, query_result.value().page_size, query_result.value().search);
    if (!result.ok()) {
      const auto error = result.error();
      return FinalizeResponse(
        request, JsonErrorResponse<personpb::ListProductsResponse>(error), started, request_id, &error);
    }

    personpb::ListProductsResponse response;
    for (const auto& product : result.value().products) {
      *response.add_products() = infra::mapping::ToProtoProduct(product);
    }
    response.set_page(result.value().page);
    response.set_page_size(result.value().page_size);
    response.set_total(result.value().total);
    response.set_query(result.value().search);
    return FinalizeResponse(request, JsonResponse(response, 200), started, request_id);
  });

  CROW_ROUTE(app, "/products/<int>").methods(crow::HTTPMethod::Get)([&product_service, &auth](
                                                                       const crow::request& request,
                                                                       const int id) {
    const auto started = std::chrono::steady_clock::now();
    const auto request_id = ResolveRequestId(request);

    auto auth_result = EnsureAuthorized(request, auth, "products.read");
    if (!auth_result.ok()) {
      const auto error = auth_result.error();
      return FinalizeResponse(
        request, JsonErrorResponse<personpb::GetProductResponse>(error), started, request_id, &error);
    }

    const auto result = product_service.GetProductById(static_cast<std::int64_t>(id));
    if (!result.ok()) {
      const auto error = result.error();
      return FinalizeResponse(
        request, JsonErrorResponse<personpb::GetProductResponse>(error), started, request_id, &error);
    }

    personpb::GetProductResponse response;
    *response.mutable_product() = infra::mapping::ToProtoProduct(result.value());
    return FinalizeResponse(request, JsonResponse(response, 200), started, request_id);
  });

  CROW_ROUTE(app, "/products").methods(crow::HTTPMethod::Post)([&product_service, &auth](
                                                                  const crow::request& request) {
    const auto started = std::chrono::steady_clock::now();
    const auto request_id = ResolveRequestId(request);

    auto auth_result = EnsureAuthorized(request, auth, "products.write");
    if (!auth_result.ok()) {
      const auto error = auth_result.error();
      return FinalizeResponse(
        request, JsonErrorResponse<personpb::CreateProductResponse>(error), started, request_id, &error);
    }

    auto parse_result = ParseJsonBody<personpb::CreateProductRequest>(request);
    if (!parse_result.ok()) {
      const auto error = parse_result.error();
      return FinalizeResponse(
        request, JsonErrorResponse<personpb::CreateProductResponse>(error), started, request_id, &error);
    }

    if (parse_result.value().product().product_type() == personpb::PRODUCT_TYPE_UNSPECIFIED) {
      const core::Error error{
        core::ErrorCode::ValidationFailed,
        "Product validation failed.",
        { "product.productType must be specified." }
      };
      return FinalizeResponse(
        request, JsonErrorResponse<personpb::CreateProductResponse>(error), started, request_id, &error);
    }

    const auto product = infra::mapping::FromProtoProductInput(parse_result.value().product());
    auto create_result = product_service.CreateProduct(product);
    if (!create_result.ok()) {
      const auto error = create_result.error();
      return FinalizeResponse(
        request, JsonErrorResponse<personpb::CreateProductResponse>(error), started, request_id, &error);
    }

    personpb::CreateProductResponse response;
    *response.mutable_product() = infra::mapping::ToProtoProduct(create_result.value());
    return FinalizeResponse(request, JsonResponse(response, 201), started, request_id);
  });

  CROW_ROUTE(app, "/products/<int>").methods(crow::HTTPMethod::Put)(
    [&product_service, &auth](const crow::request& request, const int id) {
      const auto started = std::chrono::steady_clock::now();
      const auto request_id = ResolveRequestId(request);

      auto auth_result = EnsureAuthorized(request, auth, "products.write");
      if (!auth_result.ok()) {
        const auto error = auth_result.error();
        return FinalizeResponse(
          request, JsonErrorResponse<personpb::UpdateProductResponse>(error), started, request_id, &error);
      }

      auto parse_result = ParseJsonBody<personpb::UpdateProductRequest>(request);
      if (!parse_result.ok()) {
        const auto error = parse_result.error();
        return FinalizeResponse(
          request, JsonErrorResponse<personpb::UpdateProductResponse>(error), started, request_id, &error);
      }

      if (parse_result.value().product().product_type() == personpb::PRODUCT_TYPE_UNSPECIFIED) {
        const core::Error error{
          core::ErrorCode::ValidationFailed,
          "Product validation failed.",
          { "product.productType must be specified." }
        };
        return FinalizeResponse(
          request, JsonErrorResponse<personpb::UpdateProductResponse>(error), started, request_id, &error);
      }

      const auto product = infra::mapping::FromProtoProductInput(parse_result.value().product());
      auto update_result = product_service.UpdateProduct(static_cast<std::int64_t>(id), product);
      if (!update_result.ok()) {
        const auto error = update_result.error();
        return FinalizeResponse(
          request, JsonErrorResponse<personpb::UpdateProductResponse>(error), started, request_id, &error);
      }

      personpb::UpdateProductResponse response;
      *response.mutable_product() = infra::mapping::ToProtoProduct(update_result.value());
      return FinalizeResponse(request, JsonResponse(response, 200), started, request_id);
    });

  CROW_ROUTE(app, "/products/<int>").methods(crow::HTTPMethod::Delete)([&product_service, &auth](
                                                                             const crow::request& request,
                                                                             const int id) {
    const auto started = std::chrono::steady_clock::now();
    const auto request_id = ResolveRequestId(request);

    auto auth_result = EnsureAuthorized(request, auth, "products.write");
    if (!auth_result.ok()) {
      const auto error = auth_result.error();
      return FinalizeResponse(
        request, JsonErrorResponse<personpb::DeleteProductResponse>(error), started, request_id, &error);
    }

    auto delete_result = product_service.DeleteProduct(static_cast<std::int64_t>(id));
    if (!delete_result.ok()) {
      const auto error = delete_result.error();
      return FinalizeResponse(
        request, JsonErrorResponse<personpb::DeleteProductResponse>(error), started, request_id, &error);
    }

    personpb::DeleteProductResponse response;
    response.set_deleted(delete_result.value());
    return FinalizeResponse(request, JsonResponse(response, 200), started, request_id);
  });

  CROW_ROUTE(app, "/stock").methods(crow::HTTPMethod::Get)([&inventory_service, &auth](const crow::request& request) {
    const auto started = std::chrono::steady_clock::now();
    const auto request_id = ResolveRequestId(request);

    auto auth_result = EnsureAuthorized(request, auth, "stock.read");
    if (!auth_result.ok()) {
      const auto error = auth_result.error();
      return FinalizeResponse(
        request, JsonErrorResponse<personpb::GetStockBalanceResponse>(error), started, request_id, &error);
    }

    auto query_result = ParseStockBalanceQuery(request);
    if (!query_result.ok()) {
      const auto error = query_result.error();
      return FinalizeResponse(
        request, JsonErrorResponse<personpb::GetStockBalanceResponse>(error), started, request_id, &error);
    }

    auto balance_result = inventory_service.GetStockBalance(
      query_result.value().product_id, query_result.value().warehouse_code);
    if (!balance_result.ok()) {
      const auto error = balance_result.error();
      return FinalizeResponse(
        request, JsonErrorResponse<personpb::GetStockBalanceResponse>(error), started, request_id, &error);
    }

    personpb::GetStockBalanceResponse response;
    *response.mutable_balance() = infra::mapping::ToProtoStockBalance(balance_result.value());
    return FinalizeResponse(request, JsonResponse(response, 200), started, request_id);
  });

  CROW_ROUTE(app, "/stock/movements").methods(crow::HTTPMethod::Post)([&inventory_service, &auth](
                                                                         const crow::request& request) {
    const auto started = std::chrono::steady_clock::now();
    const auto request_id = ResolveRequestId(request);

    auto auth_result = EnsureAuthorized(request, auth, "stock.write");
    if (!auth_result.ok()) {
      const auto error = auth_result.error();
      return FinalizeResponse(
        request, JsonErrorResponse<personpb::PostStockMovementResponse>(error), started, request_id, &error);
    }

    auto parse_result = ParseJsonBody<personpb::PostStockMovementRequest>(request);
    if (!parse_result.ok()) {
      const auto error = parse_result.error();
      return FinalizeResponse(
        request, JsonErrorResponse<personpb::PostStockMovementResponse>(error), started, request_id, &error);
    }

    if (parse_result.value().movement().movement_type() == personpb::STOCK_MOVEMENT_TYPE_UNSPECIFIED) {
      const core::Error error{
        core::ErrorCode::ValidationFailed,
        "Stock movement validation failed.",
        { "movement.movementType must be specified." }
      };
      return FinalizeResponse(
        request, JsonErrorResponse<personpb::PostStockMovementResponse>(error), started, request_id, &error);
    }

    const auto movement = infra::mapping::FromProtoStockMovementInput(parse_result.value().movement());
    auto movement_result = inventory_service.PostMovement(movement);
    if (!movement_result.ok()) {
      const auto error = movement_result.error();
      return FinalizeResponse(
        request, JsonErrorResponse<personpb::PostStockMovementResponse>(error), started, request_id, &error);
    }

    personpb::PostStockMovementResponse response;
    *response.mutable_balance() = infra::mapping::ToProtoStockBalance(movement_result.value());
    return FinalizeResponse(request, JsonResponse(response, 201), started, request_id);
  });

  CROW_ROUTE(app, "/pb/health").methods(crow::HTTPMethod::Get)([](const crow::request& request) {
    const auto started = std::chrono::steady_clock::now();
    const auto request_id = ResolveRequestId(request);

    personpb::HealthResponse response;
    response.set_status("ok");
    return FinalizeResponse(request, ProtobufResponse(response, 200), started, request_id);
  });

  CROW_ROUTE(app, "/pb/ready").methods(crow::HTTPMethod::Get)([&service](const crow::request& request) {
    const auto started = std::chrono::steady_clock::now();
    const auto request_id = ResolveRequestId(request);

    auto ready_check = service.ListPersons(1, 1);
    personpb::HealthResponse response;
    if (!ready_check.ok()) {
      response.set_status("not_ready");
      return FinalizeResponse(request, ProtobufResponse(response, 503), started, request_id, &ready_check.error());
    }

    response.set_status("ready");
    return FinalizeResponse(request, ProtobufResponse(response, 200), started, request_id);
  });

  CROW_ROUTE(app, "/pb/auth/status").methods(crow::HTTPMethod::Get)(
    [&auth](const crow::request& request) {
      const auto started = std::chrono::steady_clock::now();
      const auto request_id = ResolveRequestId(request);

      personpb::HealthResponse response;
      response.set_status(AuthStatusValue(auth));
      return FinalizeResponse(request, ProtobufResponse(response, 200), started, request_id);
    });

  CROW_ROUTE(app, "/pb/auth/verify").methods(crow::HTTPMethod::Get)(
    [&auth](const crow::request& request) {
      const auto started = std::chrono::steady_clock::now();
      const auto request_id = ResolveRequestId(request);

      auto auth_result = EnsureAuthenticated(request, auth);
      if (!auth_result.ok()) {
        const auto error = auth_result.error();
        return FinalizeResponse(
          request, ProtobufErrorResponse<personpb::HealthResponse>(error), started, request_id, &error);
      }

      personpb::HealthResponse response;
      response.set_status("authorized");
      return FinalizeResponse(request, ProtobufResponse(response, 200), started, request_id);
    });

  CROW_ROUTE(app, "/pb/persons").methods(crow::HTTPMethod::Get)([&service, &auth](const crow::request& request) {
    const auto started = std::chrono::steady_clock::now();
    const auto request_id = ResolveRequestId(request);

    auto auth_result = EnsureAuthorized(request, auth, "persons.read");
    if (!auth_result.ok()) {
      const auto error = auth_result.error();
      return FinalizeResponse(
        request, ProtobufErrorResponse<personpb::ListPersonsResponse>(error), started, request_id, &error);
    }

    auto query_result = ParseListQuery(request);
    if (!query_result.ok()) {
      const auto error = query_result.error();
      return FinalizeResponse(
        request, ProtobufErrorResponse<personpb::ListPersonsResponse>(error), started, request_id, &error);
    }

    const auto result = service.ListPersons(
      query_result.value().page, query_result.value().page_size, query_result.value().search);
    if (!result.ok()) {
      const auto error = result.error();
      return FinalizeResponse(
        request, ProtobufErrorResponse<personpb::ListPersonsResponse>(error), started, request_id, &error);
    }

    personpb::ListPersonsResponse response;
    for (const auto& person : result.value().persons) {
      *response.add_persons() = infra::mapping::ToProtoPerson(person);
    }
    response.set_page(result.value().page);
    response.set_page_size(result.value().page_size);
    response.set_total(result.value().total);
    response.set_query(result.value().search);
    return FinalizeResponse(request, ProtobufResponse(response, 200), started, request_id);
  });

  CROW_ROUTE(app, "/pb/persons/<int>").methods(crow::HTTPMethod::Get)([&service, &auth](
                                                                            const crow::request& request,
                                                                            const int id) {
    const auto started = std::chrono::steady_clock::now();
    const auto request_id = ResolveRequestId(request);

    auto auth_result = EnsureAuthorized(request, auth, "persons.read");
    if (!auth_result.ok()) {
      const auto error = auth_result.error();
      return FinalizeResponse(
        request, ProtobufErrorResponse<personpb::GetPersonResponse>(error), started, request_id, &error);
    }

    const auto result = service.GetPersonById(static_cast<std::int64_t>(id));
    if (!result.ok()) {
      const auto error = result.error();
      return FinalizeResponse(
        request, ProtobufErrorResponse<personpb::GetPersonResponse>(error), started, request_id, &error);
    }

    personpb::GetPersonResponse response;
    *response.mutable_person() = infra::mapping::ToProtoPerson(result.value());
    return FinalizeResponse(request, ProtobufResponse(response, 200), started, request_id);
  });

  CROW_ROUTE(app, "/pb/persons").methods(crow::HTTPMethod::Post)([&service, &auth](const crow::request& request) {
    const auto started = std::chrono::steady_clock::now();
    const auto request_id = ResolveRequestId(request);

    auto auth_result = EnsureAuthorized(request, auth, "persons.write");
    if (!auth_result.ok()) {
      const auto error = auth_result.error();
      return FinalizeResponse(
        request, ProtobufErrorResponse<personpb::CreatePersonResponse>(error), started, request_id, &error);
    }

    auto parse_result = ParseProtobufBody<personpb::CreatePersonRequest>(request);
    if (!parse_result.ok()) {
      const auto error = parse_result.error();
      return FinalizeResponse(
        request, ProtobufErrorResponse<personpb::CreatePersonResponse>(error), started, request_id, &error);
    }

    const auto person = infra::mapping::FromProtoPersonInput(parse_result.value().person());
    auto create_result = service.CreatePerson(person);
    if (!create_result.ok()) {
      const auto error = create_result.error();
      return FinalizeResponse(
        request, ProtobufErrorResponse<personpb::CreatePersonResponse>(error), started, request_id, &error);
    }

    personpb::CreatePersonResponse response;
    *response.mutable_person() = infra::mapping::ToProtoPerson(create_result.value());
    return FinalizeResponse(request, ProtobufResponse(response, 201), started, request_id);
  });

  CROW_ROUTE(app, "/pb/persons/<int>").methods(crow::HTTPMethod::Put)(
    [&service, &auth](const crow::request& request, const int id) {
      const auto started = std::chrono::steady_clock::now();
      const auto request_id = ResolveRequestId(request);

      auto auth_result = EnsureAuthorized(request, auth, "persons.write");
      if (!auth_result.ok()) {
        const auto error = auth_result.error();
        return FinalizeResponse(
          request, ProtobufErrorResponse<personpb::UpdatePersonResponse>(error), started, request_id, &error);
      }

      auto parse_result = ParseProtobufBody<personpb::UpdatePersonRequest>(request);
      if (!parse_result.ok()) {
        const auto error = parse_result.error();
        return FinalizeResponse(
          request, ProtobufErrorResponse<personpb::UpdatePersonResponse>(error), started, request_id, &error);
      }

      const auto person = infra::mapping::FromProtoPersonInput(parse_result.value().person());
      auto update_result = service.UpdatePerson(static_cast<std::int64_t>(id), person);
      if (!update_result.ok()) {
        const auto error = update_result.error();
        return FinalizeResponse(
          request, ProtobufErrorResponse<personpb::UpdatePersonResponse>(error), started, request_id, &error);
      }

      personpb::UpdatePersonResponse response;
      *response.mutable_person() = infra::mapping::ToProtoPerson(update_result.value());
      return FinalizeResponse(request, ProtobufResponse(response, 200), started, request_id);
    });

  CROW_ROUTE(app, "/pb/persons/<int>").methods(crow::HTTPMethod::Delete)([&service, &auth](
                                                                               const crow::request& request,
                                                                               const int id) {
    const auto started = std::chrono::steady_clock::now();
    const auto request_id = ResolveRequestId(request);

    auto auth_result = EnsureAuthorized(request, auth, "persons.write");
    if (!auth_result.ok()) {
      const auto error = auth_result.error();
      return FinalizeResponse(
        request, ProtobufErrorResponse<personpb::DeletePersonResponse>(error), started, request_id, &error);
    }

    auto delete_result = service.DeletePerson(static_cast<std::int64_t>(id));
    if (!delete_result.ok()) {
      const auto error = delete_result.error();
      return FinalizeResponse(
        request, ProtobufErrorResponse<personpb::DeletePersonResponse>(error), started, request_id, &error);
    }

    personpb::DeletePersonResponse response;
    response.set_deleted(delete_result.value());
    return FinalizeResponse(request, ProtobufResponse(response, 200), started, request_id);
  });

  CROW_ROUTE(app, "/pb/products").methods(crow::HTTPMethod::Get)([&product_service, &auth](
                                                                    const crow::request& request) {
    const auto started = std::chrono::steady_clock::now();
    const auto request_id = ResolveRequestId(request);

    auto auth_result = EnsureAuthorized(request, auth, "products.read");
    if (!auth_result.ok()) {
      const auto error = auth_result.error();
      return FinalizeResponse(
        request, ProtobufErrorResponse<personpb::ListProductsResponse>(error), started, request_id, &error);
    }

    auto query_result = ParseListProductsQuery(request);
    if (!query_result.ok()) {
      const auto error = query_result.error();
      return FinalizeResponse(
        request, ProtobufErrorResponse<personpb::ListProductsResponse>(error), started, request_id, &error);
    }

    const auto result = product_service.ListProducts(
      query_result.value().page, query_result.value().page_size, query_result.value().search);
    if (!result.ok()) {
      const auto error = result.error();
      return FinalizeResponse(
        request, ProtobufErrorResponse<personpb::ListProductsResponse>(error), started, request_id, &error);
    }

    personpb::ListProductsResponse response;
    for (const auto& product : result.value().products) {
      *response.add_products() = infra::mapping::ToProtoProduct(product);
    }
    response.set_page(result.value().page);
    response.set_page_size(result.value().page_size);
    response.set_total(result.value().total);
    response.set_query(result.value().search);
    return FinalizeResponse(request, ProtobufResponse(response, 200), started, request_id);
  });

  CROW_ROUTE(app, "/pb/products/<int>").methods(crow::HTTPMethod::Get)([&product_service, &auth](
                                                                          const crow::request& request,
                                                                          const int id) {
    const auto started = std::chrono::steady_clock::now();
    const auto request_id = ResolveRequestId(request);

    auto auth_result = EnsureAuthorized(request, auth, "products.read");
    if (!auth_result.ok()) {
      const auto error = auth_result.error();
      return FinalizeResponse(
        request, ProtobufErrorResponse<personpb::GetProductResponse>(error), started, request_id, &error);
    }

    const auto result = product_service.GetProductById(static_cast<std::int64_t>(id));
    if (!result.ok()) {
      const auto error = result.error();
      return FinalizeResponse(
        request, ProtobufErrorResponse<personpb::GetProductResponse>(error), started, request_id, &error);
    }

    personpb::GetProductResponse response;
    *response.mutable_product() = infra::mapping::ToProtoProduct(result.value());
    return FinalizeResponse(request, ProtobufResponse(response, 200), started, request_id);
  });

  CROW_ROUTE(app, "/pb/products").methods(crow::HTTPMethod::Post)([&product_service, &auth](
                                                                     const crow::request& request) {
    const auto started = std::chrono::steady_clock::now();
    const auto request_id = ResolveRequestId(request);

    auto auth_result = EnsureAuthorized(request, auth, "products.write");
    if (!auth_result.ok()) {
      const auto error = auth_result.error();
      return FinalizeResponse(
        request, ProtobufErrorResponse<personpb::CreateProductResponse>(error), started, request_id, &error);
    }

    auto parse_result = ParseProtobufBody<personpb::CreateProductRequest>(request);
    if (!parse_result.ok()) {
      const auto error = parse_result.error();
      return FinalizeResponse(
        request, ProtobufErrorResponse<personpb::CreateProductResponse>(error), started, request_id, &error);
    }

    if (parse_result.value().product().product_type() == personpb::PRODUCT_TYPE_UNSPECIFIED) {
      const core::Error error{
        core::ErrorCode::ValidationFailed,
        "Product validation failed.",
        { "product.productType must be specified." }
      };
      return FinalizeResponse(
        request, ProtobufErrorResponse<personpb::CreateProductResponse>(error), started, request_id, &error);
    }

    const auto product = infra::mapping::FromProtoProductInput(parse_result.value().product());
    auto create_result = product_service.CreateProduct(product);
    if (!create_result.ok()) {
      const auto error = create_result.error();
      return FinalizeResponse(
        request, ProtobufErrorResponse<personpb::CreateProductResponse>(error), started, request_id, &error);
    }

    personpb::CreateProductResponse response;
    *response.mutable_product() = infra::mapping::ToProtoProduct(create_result.value());
    return FinalizeResponse(request, ProtobufResponse(response, 201), started, request_id);
  });

  CROW_ROUTE(app, "/pb/products/<int>").methods(crow::HTTPMethod::Put)(
    [&product_service, &auth](const crow::request& request, const int id) {
      const auto started = std::chrono::steady_clock::now();
      const auto request_id = ResolveRequestId(request);

      auto auth_result = EnsureAuthorized(request, auth, "products.write");
      if (!auth_result.ok()) {
        const auto error = auth_result.error();
        return FinalizeResponse(
          request, ProtobufErrorResponse<personpb::UpdateProductResponse>(error), started, request_id, &error);
      }

      auto parse_result = ParseProtobufBody<personpb::UpdateProductRequest>(request);
      if (!parse_result.ok()) {
        const auto error = parse_result.error();
        return FinalizeResponse(
          request, ProtobufErrorResponse<personpb::UpdateProductResponse>(error), started, request_id, &error);
      }

      if (parse_result.value().product().product_type() == personpb::PRODUCT_TYPE_UNSPECIFIED) {
        const core::Error error{
          core::ErrorCode::ValidationFailed,
          "Product validation failed.",
          { "product.productType must be specified." }
        };
        return FinalizeResponse(
          request, ProtobufErrorResponse<personpb::UpdateProductResponse>(error), started, request_id, &error);
      }

      const auto product = infra::mapping::FromProtoProductInput(parse_result.value().product());
      auto update_result = product_service.UpdateProduct(static_cast<std::int64_t>(id), product);
      if (!update_result.ok()) {
        const auto error = update_result.error();
        return FinalizeResponse(
          request, ProtobufErrorResponse<personpb::UpdateProductResponse>(error), started, request_id, &error);
      }

      personpb::UpdateProductResponse response;
      *response.mutable_product() = infra::mapping::ToProtoProduct(update_result.value());
      return FinalizeResponse(request, ProtobufResponse(response, 200), started, request_id);
    });

  CROW_ROUTE(app, "/pb/products/<int>").methods(crow::HTTPMethod::Delete)([&product_service, &auth](
                                                                                const crow::request& request,
                                                                                const int id) {
    const auto started = std::chrono::steady_clock::now();
    const auto request_id = ResolveRequestId(request);

    auto auth_result = EnsureAuthorized(request, auth, "products.write");
    if (!auth_result.ok()) {
      const auto error = auth_result.error();
      return FinalizeResponse(
        request, ProtobufErrorResponse<personpb::DeleteProductResponse>(error), started, request_id, &error);
    }

    auto delete_result = product_service.DeleteProduct(static_cast<std::int64_t>(id));
    if (!delete_result.ok()) {
      const auto error = delete_result.error();
      return FinalizeResponse(
        request, ProtobufErrorResponse<personpb::DeleteProductResponse>(error), started, request_id, &error);
    }

    personpb::DeleteProductResponse response;
    response.set_deleted(delete_result.value());
    return FinalizeResponse(request, ProtobufResponse(response, 200), started, request_id);
  });

  CROW_ROUTE(app, "/pb/stock").methods(crow::HTTPMethod::Get)([&inventory_service, &auth](
                                                                 const crow::request& request) {
    const auto started = std::chrono::steady_clock::now();
    const auto request_id = ResolveRequestId(request);

    auto auth_result = EnsureAuthorized(request, auth, "stock.read");
    if (!auth_result.ok()) {
      const auto error = auth_result.error();
      return FinalizeResponse(
        request, ProtobufErrorResponse<personpb::GetStockBalanceResponse>(error), started, request_id, &error);
    }

    auto query_result = ParseStockBalanceQuery(request);
    if (!query_result.ok()) {
      const auto error = query_result.error();
      return FinalizeResponse(
        request, ProtobufErrorResponse<personpb::GetStockBalanceResponse>(error), started, request_id, &error);
    }

    auto balance_result = inventory_service.GetStockBalance(
      query_result.value().product_id, query_result.value().warehouse_code);
    if (!balance_result.ok()) {
      const auto error = balance_result.error();
      return FinalizeResponse(
        request, ProtobufErrorResponse<personpb::GetStockBalanceResponse>(error), started, request_id, &error);
    }

    personpb::GetStockBalanceResponse response;
    *response.mutable_balance() = infra::mapping::ToProtoStockBalance(balance_result.value());
    return FinalizeResponse(request, ProtobufResponse(response, 200), started, request_id);
  });

  CROW_ROUTE(app, "/pb/stock/movements").methods(crow::HTTPMethod::Post)([&inventory_service, &auth](
                                                                            const crow::request& request) {
    const auto started = std::chrono::steady_clock::now();
    const auto request_id = ResolveRequestId(request);

    auto auth_result = EnsureAuthorized(request, auth, "stock.write");
    if (!auth_result.ok()) {
      const auto error = auth_result.error();
      return FinalizeResponse(
        request, ProtobufErrorResponse<personpb::PostStockMovementResponse>(error), started, request_id, &error);
    }

    auto parse_result = ParseProtobufBody<personpb::PostStockMovementRequest>(request);
    if (!parse_result.ok()) {
      const auto error = parse_result.error();
      return FinalizeResponse(
        request, ProtobufErrorResponse<personpb::PostStockMovementResponse>(error), started, request_id, &error);
    }

    if (parse_result.value().movement().movement_type() == personpb::STOCK_MOVEMENT_TYPE_UNSPECIFIED) {
      const core::Error error{
        core::ErrorCode::ValidationFailed,
        "Stock movement validation failed.",
        { "movement.movementType must be specified." }
      };
      return FinalizeResponse(
        request, ProtobufErrorResponse<personpb::PostStockMovementResponse>(error), started, request_id, &error);
    }

    const auto movement = infra::mapping::FromProtoStockMovementInput(parse_result.value().movement());
    auto movement_result = inventory_service.PostMovement(movement);
    if (!movement_result.ok()) {
      const auto error = movement_result.error();
      return FinalizeResponse(
        request, ProtobufErrorResponse<personpb::PostStockMovementResponse>(error), started, request_id, &error);
    }

    personpb::PostStockMovementResponse response;
    *response.mutable_balance() = infra::mapping::ToProtoStockBalance(movement_result.value());
    return FinalizeResponse(request, ProtobufResponse(response, 201), started, request_id);
  });

  CROW_CATCHALL_ROUTE(app)([](const crow::request& request) {
    const auto started = std::chrono::steady_clock::now();
    const auto request_id = ResolveRequestId(request);

    const core::Error error{
      core::ErrorCode::NotFound,
      "Route was not found.",
      {}
    };

    const auto accept = request.get_header_value("Accept");
    const bool wants_protobuf =
      request.url.rfind("/pb/", 0) == 0 || accept.find("application/x-protobuf") != std::string::npos;

    if (wants_protobuf) {
      return FinalizeResponse(
        request, ProtobufErrorResponse<personpb::HealthResponse>(error), started, request_id, &error);
    }
    return FinalizeResponse(
      request, JsonErrorResponse<personpb::HealthResponse>(error), started, request_id, &error);
  });

  std::cout << "API started: http://127.0.0.1:" << port << '\n';
  std::cout << "SQLite database: " << database_path << '\n';
  std::cout << "Concurrency: " << concurrency << '\n';
  std::cout << "Runtime env: " << RuntimeEnvironmentToString(runtime_environment) << '\n';
  std::cout << "Token auth: " << (auth.enabled ? "enabled" : "disabled") << '\n';
  std::cout << "RBAC authz: " << (auth.authorization_enabled ? "enabled" : "disabled") << '\n';
  if (auth.enabled) {
    std::cout << "Auth token count: " << auth.tokens.size() << '\n';
  }
  if (auth.authorization_enabled) {
    std::cout << "Token permission mappings: " << auth.token_permissions.size() << '\n';
  }

  try {
    app.port(port).concurrency(concurrency).run();
  } catch (const std::exception& exception) {
    std::cerr << "API runtime exception: " << exception.what() << '\n';
    return 1;
  }
  return 0;
}
