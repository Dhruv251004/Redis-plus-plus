#pragma once

constexpr int64_t NO_EXPIRY = INT64_MAX;

bool is_expired(int64_t expiry);

int64_t current_time_ms();

int64_t absolute_time_ms(int64_t);

int64_t seconds_to_ms(int64_t);
