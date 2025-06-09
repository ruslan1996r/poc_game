#include <iostream>
#include <unordered_map>
#include <vector>
#include <cstring>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <arpa/inet.h>
#include <unistd.h>

const int SERVER_PORT = 12345;
const char *SERVER_IP = "127.0.0.1";
const int BUFFER_SIZE = 512;
const int WINDOW_SIZE = 5;
const int BASE_TIMEOUT_MS = 200;
const int EXTENDED_TIMEOUT_MS = 500;
const int MAX_RETRIES = 5;
const int MESSAGES_TO_SEND = 10;

struct Packet {
  uint32_t sessionId;
  uint32_t id;
  char data[BUFFER_SIZE];
};

struct AckPacket {
  uint32_t sessionId;
  uint32_t firstPacketId;
  std::bitset<WINDOW_SIZE> receivedMask;
};

struct SentPacketInfo {
  Packet packet;
  int retries = 0;
  std::chrono::steady_clock::time_point lastSent;
};

int main() {
  srand(time(nullptr));
  uint32_t sessionId = rand();

  int sockfd;
  sockaddr_in serverAddr{};
  socklen_t serverLen = sizeof(serverAddr);
  AckPacket ack{};

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    std::cerr << "Ошибка при создании сокета" << std::endl;
    return 1;
  }

  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(SERVER_PORT);
  inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

  struct timeval timeout{};
  timeout.tv_sec = 0;
  timeout.tv_usec = BASE_TIMEOUT_MS * 1000;
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

  std::unordered_map<uint32_t, SentPacketInfo> sentPackets;

  for (uint32_t packetId = 1; packetId <= MESSAGES_TO_SEND; packetId++) {
    Packet packet{};
    packet.sessionId = sessionId;
    packet.id = packetId;
    snprintf(packet.data, BUFFER_SIZE, "Сообщение #%u", packet.id);

    SentPacketInfo info;
    info.packet = packet;
    info.lastSent = std::chrono::steady_clock::now();

    sentPackets[packetId] = info;

    sendto(sockfd, &packet, sizeof(packet), 0, (sockaddr *)&serverAddr, serverLen);
    std::cout << "Отправлено [" << packet.id << "]: " << packet.data << std::endl;
  }

  while (!sentPackets.empty()) {
    ssize_t received = recvfrom(sockfd, &ack, sizeof(ack), 0, (sockaddr *)&serverAddr, &serverLen);
    if (received > 0) {
      if (ack.sessionId != sessionId) {
        std::cout << "\033[31mПропущен ACK от другой сессии (ID: " << ack.sessionId << ")\033[0m" << std::endl;
        continue;
      }

      std::cout << "Получен Batch ACK c ID " << ack.firstPacketId << ", mask: " << ack.receivedMask << std::endl;

      for (int i = 0; i < WINDOW_SIZE; i++) {
        if (ack.receivedMask[i]) {
          uint32_t ackedId = ack.firstPacketId + i;
          sentPackets.erase(ackedId);
        }
      }
    }

    auto now = std::chrono::steady_clock::now();
    std::vector<uint32_t> toRemove;

    for (auto &[id, info] : sentPackets) {
      int currentTimeout = (info.retries < 3) ? BASE_TIMEOUT_MS : EXTENDED_TIMEOUT_MS;
      int elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - info.lastSent).count();

      if (elapsedMs >= currentTimeout) {
        if (info.retries >= MAX_RETRIES) {
          std::cerr << "\033[31mПакет " << id << " не был подтверждён после " << MAX_RETRIES << " попыток\033[0m" << std::endl;
          toRemove.push_back(id);
        } else {
          sendto(sockfd, &info.packet, sizeof(info.packet), 0, (sockaddr *)&serverAddr, serverLen);
          info.lastSent = now;
          info.retries++;
          std::cout << "\033[33mПовторная отправка [" << id << "], попытка " << info.retries
                    << ", таймаут: " << currentTimeout << " мс\033[0m" << std::endl;
        }
      }
    }

    for (uint32_t id : toRemove) {
      sentPackets.erase(id);
    }

    usleep(10 * 1000); // 10 мс
  }

  close(sockfd);
  std::cout << "\033[32mВсе пакеты подтверждены или отбраковались после ретраев\033[0m" << std::endl;
  return 0;
}
