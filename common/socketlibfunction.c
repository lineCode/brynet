#include "fdset.h"
#include "socketlibfunction.h"

#if defined PLATFORM_WINDOWS
static WSADATA g_WSAData;
#endif

void 
ox_socket_init(void)
{
    #if defined PLATFORM_WINDOWS
    static bool WinSockIsInit = false;
    if(!WinSockIsInit)
    {
        WSAStartup(MAKEWORD(2,2), &g_WSAData);
        WinSockIsInit = true;
    }
    #endif
}

void 
ox_socket_destroy(void)
{
    #if defined PLATFORM_WINDOWS
    WSACleanup();
    #endif
}

int
ox_socket_nodelay(sock fd)
{
    int flag = 1;
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));
}

bool 
ox_socket_nonblock(sock fd)
{
    int err;
    unsigned long ul = true;
    #if defined PLATFORM_WINDOWS
    err = ioctlsocket(fd, FIONBIO, &ul);
    #else
    err = ioctl(fd, FIONBIO, &ul);
    #endif

    return err != SOCKET_ERROR;
}

int
ox_socket_setsdsize(sock fd, int sd_size)
{
    return setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (const char*)&sd_size, sizeof(sd_size));
}

int
ox_socket_setrdsize(sock fd, int rd_size)
{
    return setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (const char*)&rd_size, sizeof(rd_size));
}

sock 
ox_socket_connect(const char* server_ip, int port)
{
    struct sockaddr_in server_addr;
    sock clientfd = SOCKET_ERROR;

    ox_socket_init();

    clientfd = socket(AF_INET, SOCK_STREAM, 0);

    if(clientfd != SOCKET_ERROR)
    {
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(server_ip);
        server_addr.sin_port = htons(port);

        while(connect(clientfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) < 0)
        {
            if(EINTR == sErrno)
            {
                continue;
            }
        }
    }

    return clientfd;
}

sock
ox_socket_nonblock_connect(const char* server_ip, int port, int second)
{
	struct sockaddr_in server_addr;
    sock clientfd = SOCKET_ERROR;
    bool connect_ret = true;

    ox_socket_init();

    clientfd = socket(AF_INET, SOCK_STREAM, 0);
    ox_socket_nonblock(clientfd);

    if(clientfd != SOCKET_ERROR)
    {
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(server_ip);
        server_addr.sin_port = htons(port);

        if(connect(clientfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) < 0)
        {
            int check_error = 0;
            
            #if defined PLATFORM_WINDOWS
            check_error = WSAEWOULDBLOCK;
            #else
            check_error = EINPROGRESS;
            #endif

            if(check_error != sErrno)
            {
                connect_ret = false;
            }
            else
            {
                struct fdset_s* fdset = ox_fdset_new();
                bool canwrite = false;
                bool canread = false;
                /*  ����Ĭ������״̬Ϊʧ��   */
                connect_ret = false;

                ox_fdset_add(fdset, clientfd, ReadCheck | WriteCheck);
                ox_fdset_poll(fdset, second);

                canwrite = ox_fdset_check(fdset, clientfd, WriteCheck);
                canread = ox_fdset_check(fdset, clientfd, ReadCheck);

                if(canwrite)
                {
                    if(canread)
                    {
                        int error;
                        int len = sizeof(error);
                        if(getsockopt(clientfd, SOL_SOCKET, SO_ERROR, (char*)&error, (socklen_t*)&len) >= 0)
                        {
                            /*  ����ֵ��Ϊ-1(linux��ʧ��Ҳ����0,�����������)��Ϊ�ɹ� */
                            connect_ret = true;
                        }
                    }
                    else
                    {
                        /*  �ɶ��Ҳ���д��Ϊ���ӳɹ�    */
                        connect_ret = true;
                    }
                }

                ox_fdset_delete(fdset);
            }
        }
    }

    if(!connect_ret)
    {
        ox_socket_close(clientfd);
        clientfd = SOCKET_ERROR;
    }

    return clientfd;
}

sock 
ox_socket_listen(int port, int back_num)
{
    sock socketfd = SOCKET_ERROR;
    struct  sockaddr_in server_addr;
    int reuseaddr_value = 1;

    ox_socket_init();

    socketfd = socket(AF_INET, SOCK_STREAM, 0);

    if(socketfd != SOCKET_ERROR)
    {
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        server_addr.sin_addr.s_addr = INADDR_ANY;

        setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuseaddr_value , sizeof(int));

        if( bind(socketfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) == SOCKET_ERROR||
            listen(socketfd, back_num) == SOCKET_ERROR)
        {
            ox_socket_close(socketfd);
            socketfd = SOCKET_ERROR;
        }
    }
    
    
    return socketfd;
}

void 
ox_socket_close(sock fd)
{
    #if defined PLATFORM_WINDOWS
    closesocket(fd);
    #else
    close(fd);
    #endif
}

const char* 
ox_socket_getipstr(unsigned int ip)
{
    struct in_addr addr;
    addr.s_addr = htonl(ip);
    return inet_ntoa(addr);
}

int 
ox_socket_send(sock fd, const char* buffer, int len)
{
    int transnum = send(fd, buffer, len, 0);
    if(transnum < 0 && S_EWOULDBLOCK == sErrno)
    {
        transnum = 0;
    }

    /*  send error if transnum < 0  */
    return transnum;
}