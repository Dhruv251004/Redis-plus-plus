#pragma once

constexpr int64_t NO_EXPIRY = INT64_MAX;

bool isExpired(int64_t expiry);

int64_t currentTimeMs();