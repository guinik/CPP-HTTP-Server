FROM gcc:14

RUN apt-get update && apt-get install -y cmake && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --config Release

WORKDIR /app/build

COPY public/ ./public/

EXPOSE 2700

CMD ["./http_server", "2700"]
