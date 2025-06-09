#include <iostream>
#include <unordered_set>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

const int PORT = 12345;
const int BUFFER_SIZE = 512;

struct Packet
{
  uint32_t id;
  char data[BUFFER_SIZE];
};

int main()
{
  int sockfd;
  sockaddr_in serverAddr{}, clientAddr{};
  socklen_t clientLen = sizeof(clientAddr);
  Packet packet{};
  std::unordered_set<uint32_t> receivedIds; // Запоминаем полученные ID

  // 1. Создаём сокет
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0)
  {
    std::cerr << "Ошибка при создании сокета" << std::endl;
    return 1;
  }

  // 2. Настраиваем серверный адрес
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  serverAddr.sin_port = htons(PORT);

  // 3. Привязываем сокет к порту
  if (bind(sockfd, (sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
  {
    std::cerr << "Ошибка при привязке сокета" << std::endl;
    return 1;
  }

  std::cout << "UDP сервер запущен на порту " << PORT << std::endl;

  while (true)
  {
    // 4. Получаем пакет
    ssize_t received = recvfrom(sockfd, &packet, sizeof(packet), 0, (sockaddr *)&clientAddr, &clientLen);
    if (received > 0)
    {
      packet.data[BUFFER_SIZE - 1] = '\0';
      std::cout << "Получено [" << packet.id << "]: " << packet.data << std::endl;

      // 5. Проверяем, получали ли мы этот пакет ранее
      if (receivedIds.count(packet.id)) // count - вернёт 0/1, если элемент был найден или нет
      {
        std::cout << "Дубликат пакета [" << packet.id << "], игнорируем." << std::endl;
        continue;
      }

      receivedIds.insert(packet.id); // Запоминаем ID пакета

      // 6. Отправляем ACK с номером пакета
      sendto(sockfd, &packet.id, sizeof(packet.id), 0, (sockaddr *)&clientAddr, clientLen);
    }
  }

  close(sockfd);
  return 0;
}
