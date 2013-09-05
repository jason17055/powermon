#include <winsock2.h>
#include <ws2tcpip.h>
#include <tchar.h>

static SOCKET sockfd;
static int arg_level = 1;
static BOOL arg_hide = FALSE;

static void
shift_args(int *pargc, const TCHAR *argv[])
{
	int i;

	(*pargc)--;
	for (i = 1; i < *pargc; i++) {
		argv[i] = argv[i+1];
	}
}

static void
process_args(int *pargc, const TCHAR *argv[])
{
	while (*pargc >= 2) {
		if (argv[1][0] != '-') {
			break;
		}

		if (_tcscmp(argv[1], TEXT("-2")) == 0) {
			arg_level = 2;
			shift_args(pargc, argv);
		}
		else if (_tcscmp(argv[1], TEXT("-hide")) == 0) {
			arg_hide = TRUE;
			shift_args(pargc, argv);
		}
		else {
			fprintf(stderr, "Error: unrecognized option: %S\n", argv[1]);
			exit(2);
		}
	}
}

int
_tmain(int argc, TCHAR *argv[])
{
	struct sockaddr_in servaddr;

	process_args(&argc, argv);

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
		if (arg_hide) {
			_snprintf(buf, 1500, "-hide");
		}
		else {
			_snprintf(buf, 1500, "%d%S",
				arg_level,
				argc >= 2 ? argv[1] : TEXT("")
				);
		}

		sendto(sockfd, buf, strlen(buf),
			0, /* flags */
			(struct sockaddr *)&servaddr,
			sizeof(servaddr)
			);
	}
}
