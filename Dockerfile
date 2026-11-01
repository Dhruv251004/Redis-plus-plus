FROM ubuntu:26.04 AS build
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git ca-certificates zlib1g-dev libgtest-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY CMakeLists.txt .
COPY src ./src
COPY tests ./tests
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --target redispp -j"$(nproc)"

FROM ubuntu:26.04
RUN apt-get update && apt-get install -y --no-install-recommends zlib1g \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /app
RUN mkdir data
COPY --from=build /src/build/redispp /usr/local/bin/redispp
VOLUME /app/data
EXPOSE 6379
CMD ["redispp"]
