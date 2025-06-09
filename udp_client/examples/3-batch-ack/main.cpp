#include <iostream>
#include <unordered_map>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

const int SERVER_PORT = 12345;
const char *SERVER_IP = "127.0.0.1";
const int BUFFER_SIZE = 512;
const int WINDOW_SIZE = 5;
const int TIMEOUT_MS = 200;
const int MAX_RETRIES = 3;

struct Packet
{
  uint32_t id;
  char data[BUFFER_SIZE];
};

struct AckPacket
{
  uint32_t firstPacketId;
  std::bitset<WINDOW_SIZE> receivedMask;
};

int main()
{
  int sockfd;
  sockaddr_in serverAddr{};
  socklen_t serverLen = sizeof(serverAddr);
  std::unordered_map<uint32_t, Packet> sentPackets;
  AckPacket ack{};

  // 1. Создаём сокет
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0)
  {
    std::cerr << "Ошибка при создании сокета" << std::endl;
    return 1;
  }

  // 2. Настраиваем серверный адрес
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(SERVER_PORT);
  inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

  // 3. Устанавливаем таймаут для ожидания ACK
  struct timeval timeout{};
  timeout.tv_sec = 0;
  timeout.tv_usec = TIMEOUT_MS * 1000;
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

  // 4. Отправляем пакеты
  for (uint32_t packetId = 1; packetId <= 10; packetId++)
  {
    Packet packet{};
    packet.id = packetId;
    snprintf(packet.data, BUFFER_SIZE, "Сообщение #%u", packet.id);
    sentPackets[packet.id] = packet;

    sendto(sockfd, &packet, sizeof(packet), 0, (sockaddr *)&serverAddr, serverLen);
    std::cout << "Отправлено [" << packet.id << "]: " << packet.data << std::endl;

    // Выполнится на каждое 5-е сообщение (как раз размер маски)
    if (packetId % WINDOW_SIZE == 0)
    {
      // Ждём Batch ACK. Получит сразу пачку подтверждённых сообщений (5 штук, &ack)
      ssize_t received = recvfrom(sockfd, &ack, sizeof(ack), 0, (sockaddr *)&serverAddr, &serverLen);

      if (received > 0)
      {
        std::cout << "Получен Batch ACK c ID " << ack.firstPacketId << std::endl;

        for (int i = 0; i < WINDOW_SIZE; i++)
        {
          if (ack.receivedMask[i])
          {
            sentPackets.erase(ack.firstPacketId + i); // Удаляем подтверждённые пакеты
          }
        }
      }

      if (received == -1)
      {
        std::cerr << "\033[31mBatch ACK '" << ack.firstPacketId << "' не был получен\033[0m" << std::endl;
      }
    }
  }

  close(sockfd);
  return 0;
}
