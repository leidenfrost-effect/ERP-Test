#pragma once

#include <string>
#include <vector>

namespace core {

enum class ErrorCode {
  ValidationFailed,
  NotFound,
  RepositoryError,
  ParseError,
  Unauthorized,
  Forbidden
};

inline std::string ToString(ErrorCode code) {
  switch (code) {
    case ErrorCode::ValidationFailed:
      return "VALIDATION_FAILED";
    case ErrorCode::NotFound:
      return "NOT_FOUND";
    case ErrorCode::RepositoryError:
      return "REPOSITORY_ERROR";
    case ErrorCode::ParseError:
      return "PARSE_ERROR";
    case ErrorCode::Unauthorized:
      return "UNAUTHORIZED";
    case ErrorCode::Forbidden:
      return "FORBIDDEN";
  }
  return "UNKNOWN_ERROR";
}

struct Error {
  ErrorCode code{ErrorCode::RepositoryError};
  std::string message;
  std::vector<std::string> details;
};

}  // namespace core
