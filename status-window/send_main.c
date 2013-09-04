#include <winsock2.h>
#include <ws2tcpip.h>
#include <tchar.h>

static SOCKET sockfd;

int
_tmain(int argc, TCHAR *argv[])
{
	struct sockaddr_in servaddr;

	/* initialize WinSock */
	{
		WSADATA wsaData = {0};
		WSAStartup(MAKEWORD(2,2), &wsaData);
	}

	/* create udp socket */
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	/* setup server address structure */
	ZeroMemory(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	servaddr.sin_port = htons(56599);

	/* send a message */
	{
		char buf[1500];
		_snprintf(buf, 1500, "1%S", argv[1]);

		sendto(sockfd, buf, strlen(buf),
			0, /* flags */
			(struct sockaddr *)&servaddr,
			sizeof(servaddr)
			);
	}
}
