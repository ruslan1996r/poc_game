#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

const int PORT = 12345;
const int BUFFER_SIZE = 1024;

int main()
{
  int sockfd;
  sockaddr_in serverAddr{}, clientAddr{};
  socklen_t clientLen = sizeof(clientAddr);
  char buffer[BUFFER_SIZE];

  // 1. Создаём сокет
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0)
  {
    std::cerr << "Ошибка при создании сокета" << std::endl;
    return 1;
  }

  // 2. Настраиваем адрес сервера
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
    // 4. Принимаем данные от клиента
    ssize_t received = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0, (sockaddr *)&clientAddr, &clientLen);
    if (received > 0)
    {
      buffer[received] = '\0';
      std::cout << "Получено: " << buffer << std::endl;

      // 5. Отправляем ACK обратно клиенту
      const char *ack = "ACK";
      sendto(sockfd, ack, strlen(ack), 0, (sockaddr *)&clientAddr, clientLen);
    }
  }

  close(sockfd);
  return 0;
}
