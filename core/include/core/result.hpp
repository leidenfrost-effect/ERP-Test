#pragma once

#include <optional>
#include <stdexcept>
#include <utility>

#include "core/error.hpp"

namespace core {

template <typename T>
class Result {
 public:
  static Result<T> Success(T value) { return Result<T>(std::move(value)); }

  static Result<T> Failure(Error error) { return Result<T>(std::move(error)); }

  bool ok() const { return value_.has_value(); }

  const T& value() const {
    if (!value_.has_value()) {
      throw std::logic_error("Result does not contain a value.");
    }
    return *value_;
  }

  T& value() {
    if (!value_.has_value()) {
      throw std::logic_error("Result does not contain a value.");
    }
    return *value_;
  }

  const Error& error() const {
    if (!error_.has_value()) {
      throw std::logic_error("Result does not contain an error.");
    }
    return *error_;
  }

 private:
  explicit Result(T value) : value_(std::move(value)) {}

  explicit Result(Error error) : error_(std::move(error)) {}

  std::optional<T> value_;
  std::optional<Error> error_;
};

}  // namespace core
