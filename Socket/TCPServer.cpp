/**
* @file TCPServer.cpp
* @brief implementation of the TCP server class
* @author Mohamed Amine Mzoughi <mohamed-amine.mzoughi@laposte.net>
*/

#include "TCPServer.h"


CTCPServer::CTCPServer(const LogFnCallback oLogger,
                       /*const std::string& strAddr,*/
                       const std::string& strPort,
                       const SettingsFlag eSettings /*= ALL_FLAGS*/)
                       throw (EResolveError) :
   ASocket(oLogger, eSettings),
   m_ListenSocket(INVALID_SOCKET),
   #ifdef WINDOWS
   m_pResultAddrInfo(nullptr),
   #endif
   //m_strHost(strAddr),
   m_strPort(strPort)
{
   #ifdef WINDOWS
   // Resolve the server address and port
   ZeroMemory(&m_HintsAddrInfo, sizeof(m_HintsAddrInfo));
   /* AF_INET is used to specify the IPv4 address family. */
   m_HintsAddrInfo.ai_family = AF_INET;
   /* SOCK_STREAM is used to specify a stream socket. */
   m_HintsAddrInfo.ai_socktype = SOCK_STREAM;
   /* IPPROTO_TCP is used to specify the TCP protocol. */
   m_HintsAddrInfo.ai_protocol = IPPROTO_TCP;
   /* AI_PASSIVE flag indicates the caller intends to use the returned socket
   * address structure in a call to the bind function.*/
   m_HintsAddrInfo.ai_flags = AI_PASSIVE;

   int iResult = getaddrinfo(nullptr, strPort.c_str(), &m_HintsAddrInfo, &m_pResultAddrInfo);
   if (iResult != 0)
   {
      if (m_pResultAddrInfo != nullptr)
      {
         freeaddrinfo(m_pResultAddrInfo);
         m_pResultAddrInfo = nullptr;
      }

      throw EResolveError(StringFormat("[TCPServer][Error] getaddrinfo failed : %d", iResult));
   }
   #else
   // clear address structure
   bzero((char *) &m_ServAddr, sizeof(m_ServAddr));

   int iPort = atoi(strPort.c_str());

   /* setup the host_addr structure for use in bind call */
   // server byte order
   m_ServAddr.sin_family = AF_INET;  

   // automatically be filled with current host's IP address
   m_ServAddr.sin_addr.s_addr = INADDR_ANY;
   //m_ServAddr.sin_addr.s_addr = inet_addr(strAddr.c_str()); // doesn't work !

   // convert short integer value for port must be converted into network byte order
   m_ServAddr.sin_port = htons(iPort);
   #endif
}

// returns the socket of the accepted client
bool CTCPServer::Listen(ASocket::Socket& ClientSocket)
{
   ClientSocket = INVALID_SOCKET;

   // creates a socket to listen for incoming client connections if it doesn't already exist
   if (m_ListenSocket == INVALID_SOCKET)
   {
      #ifdef WINDOWS
      m_ListenSocket = socket(m_pResultAddrInfo->ai_family,
                              m_pResultAddrInfo->ai_socktype,
                              m_pResultAddrInfo->ai_protocol);
      
      if (m_ListenSocket == INVALID_SOCKET)
      {
         if (m_eSettingsFlags & ENABLE_LOG)
            m_oLog(StringFormat("[TCPServer][Error] socket failed : %d", WSAGetLastError()));
         freeaddrinfo(m_pResultAddrInfo);
         m_pResultAddrInfo = nullptr;
         return false;
      }

      // bind the listen socket to the host address:port
      int iResult = bind(m_ListenSocket,
                         m_pResultAddrInfo->ai_addr,
                         static_cast<int>(m_pResultAddrInfo->ai_addrlen));
      
      freeaddrinfo(m_pResultAddrInfo);	// free memory allocated by getaddrinfo
      m_pResultAddrInfo = nullptr;

      if (iResult == SOCKET_ERROR)
      {
         if (m_eSettingsFlags & ENABLE_LOG)
            m_oLog(StringFormat("[TCPServer][Error] bind failed : %d", WSAGetLastError()));
         closesocket(m_ListenSocket);
         m_ListenSocket = INVALID_SOCKET;
         return false;
      }
      #else

      // create a socket
      // socket(int domain, int type, int protocol)
      m_ListenSocket =  socket(AF_INET, SOCK_STREAM, 0/*IPPROTO_TCP*/);
      if (m_ListenSocket < 0)
      {
         if (m_eSettingsFlags & ENABLE_LOG)
            m_oLog(StringFormat("[TCPServer][Error] opening socket : %s", strerror(errno)));
         m_ListenSocket = INVALID_SOCKET;
         return false;
      }

      // bind(int fd, struct sockaddr *local_addr, socklen_t addr_length)
      // bind() passes file descriptor, the address structure, 
      // and the length of the address structure
      // This bind() call will bind  the socket to the current IP address on port, portno
      int iResult = bind(m_ListenSocket,
                         reinterpret_cast<struct sockaddr*>(&m_ServAddr),
                         sizeof(m_ServAddr));
      if (iResult < 0)
      {
         if (m_eSettingsFlags & ENABLE_LOG)
            m_oLog(StringFormat("[TCPServer][Error] bind failed : %s", strerror(errno)));
         return false;
      }
      #endif
   }


   #ifdef WINDOWS
   sockaddr addrClient;
   int iResult;
   /* SOMAXCONN = allow max number of connexions in waiting */
   iResult = listen(m_ListenSocket, SOMAXCONN);
   if (iResult == SOCKET_ERROR)
   {
      if (m_eSettingsFlags & ENABLE_LOG)
         m_oLog(StringFormat("[TCPServer][Error] listen failed : %d", WSAGetLastError()));
      closesocket(m_ListenSocket);
      m_ListenSocket = INVALID_SOCKET;
      return false;
   }

   // accept client connection, the returned socket will be used for I/O operations
   int iAddrLen = sizeof(addrClient);
   ClientSocket = accept(m_ListenSocket, &addrClient, &iAddrLen);
   if (ClientSocket == INVALID_SOCKET)
   {
      if (m_eSettingsFlags & ENABLE_LOG)
         m_oLog(StringFormat("[TCPServer][Error] accept failed : %d", WSAGetLastError()));

      return false;
   }

   {
      if (m_eSettingsFlags & ENABLE_LOG)
         // TODO : a version that handles IPv6
         m_oLog( StringFormat("[TCPServer][Info] Incoming connection from '%s' port '%d'",
              (addrClient.sa_family == AF_INET) ? inet_ntoa(((struct sockaddr_in*)&addrClient)->sin_addr) : "",
              (addrClient.sa_family == AF_INET) ? ntohs(((struct sockaddr_in*)&addrClient)->sin_port) : 0));
   }

   //char buf1[256];
   //unsigned long len2 = 256UL;
   //if (!WSAAddressToStringA(&addrClient, lenAddr, NULL, buf1, &len2))
      //if (m_eSettingsFlags & ENABLE_LOG)
         //m_oLog(StringFormat("[TCPServer][Info] Connection from %s", buf1));

   #else
   // This listen() call tells the socket to listen to the incoming connections.
   // The listen() function places all incoming connection into a backlog queue
   // until accept() call accepts the connection.
   // Here, we set the maximum size for the backlog queue to SOMAXCONN.
   int iResult = listen(m_ListenSocket, SOMAXCONN);
   if (iResult < 0)
   {
      if (m_eSettingsFlags & ENABLE_LOG)
         m_oLog(StringFormat("[TCPServer][Error] listen failed : %s", strerror(errno)));

      return false;
   }

   struct sockaddr_in ClientAddr;
   // The accept() call actually accepts an incoming connection
   socklen_t uClientLen = sizeof(ClientAddr);

   // This accept() function will write the connecting client's address info 
   // into the the address structure and the size of that structure is uClientLen.
   // The accept() returns a new socket file descriptor for the accepted connection.
   // So, the original socket file descriptor can continue to be used 
   // for accepting new connections while the new socker file descriptor is used for
   // communicating with the connected client.
   ClientSocket = accept(m_ListenSocket,
                         reinterpret_cast<struct sockaddr*>(&ClientAddr),
                         &uClientLen);

   if (ClientSocket < 0)
   {
      if (m_eSettingsFlags & ENABLE_LOG)
         m_oLog(StringFormat("[TCPServer][Error] accept failed : %s", strerror(errno)));

      return false;
   }

   if (m_eSettingsFlags & ENABLE_LOG)
      m_oLog(StringFormat("[TCPServer][Info] Incoming connection from '%s' port '%d'",
             inet_ntoa(ClientAddr.sin_addr), ntohs(ClientAddr.sin_port)));
   #endif

   return true;
}

/* ret > 0   : bytes received
 * ret == 0  : connection closed
 * ret < 0   : recv failed
 */
int CTCPServer::Receive(const CTCPServer::Socket ClientSocket, char* pData, const size_t uSize) const
{
   int iBytesRcvd;

   #ifdef WINDOWS
   iBytesRcvd = recv(ClientSocket, pData, uSize, 0);
   if (iBytesRcvd < 0)
   {
      if (m_eSettingsFlags & ENABLE_LOG)
         m_oLog(StringFormat("[TCPServer][Error] recv failed : %d", WSAGetLastError()));
   }
   #else
   iBytesRcvd = read(ClientSocket, pData, uSize);
   if (iBytesRcvd < 0)
   {
      if (m_eSettingsFlags & ENABLE_LOG)
         m_oLog(StringFormat("[TCPServer][Error] reading from socket : %s", strerror(errno)));
   }
   #endif

   return iBytesRcvd;
}

bool CTCPServer::Send(const Socket ClientSocket, const char* pData, size_t uSize) const
{
   int iResult = 0;

   #ifdef WINDOWS
   iResult = send(ClientSocket, pData, uSize, 0);
   if (iResult == SOCKET_ERROR)
   {
      if (m_eSettingsFlags & ENABLE_LOG)
         m_oLog(StringFormat("[TCPServer][Error] send failed : %d", WSAGetLastError()));
      //Disconnect();
      return false;
   }
   #else

   //send(ClientSocket, pData, uSize, 0);
   iResult = write(ClientSocket, pData, uSize);
   if (iResult < 0) 
   {
      if (m_eSettingsFlags & ENABLE_LOG)
         m_oLog(StringFormat("[TCPServer][Error] writing to socket : %s", strerror(errno)));

      return false;
   }
   #endif
   
   return true;
}

bool CTCPServer::Send(const Socket ClientSocket, const std::string& strData) const
{
   return Send(ClientSocket, strData.c_str(), strData.length());
}

bool CTCPServer::Send(const Socket ClientSocket, const std::vector<char>& Data) const
{
   return Send(ClientSocket, Data.data(), Data.size());
}

bool CTCPServer::Disconnect(const CTCPServer::Socket ClientSocket) const
{
   #ifdef WINDOWS
   // The shutdown function disables sends or receives on a socket.
   int iResult = shutdown(ClientSocket, SD_RECEIVE);
   
   if (iResult == SOCKET_ERROR)
   {
      if (m_eSettingsFlags & ENABLE_LOG)
         m_oLog(StringFormat("[TCPServer][Error] shutdown failed : %d", WSAGetLastError()));

      return false;
   }

   closesocket(ClientSocket);
   #else

   close(ClientSocket);

   #endif

   return true;
}

CTCPServer::~CTCPServer()
{
   #ifdef WINDOWS
   // close listen socket
   closesocket(m_ListenSocket);
   #else
   close(m_ListenSocket);
   #endif
}
