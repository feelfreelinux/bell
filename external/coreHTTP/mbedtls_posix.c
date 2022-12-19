/*
 * AWS IoT Device SDK for Embedded C 202108.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Standard includes. */
#include <assert.h>
#include <string.h>

/* POSIX socket includes. */
#include <errno.h>
#include <sys/socket.h>

#include "mbedtls_posix.h"
#include "sockets_posix.h"
#include "transport_interface.h"

/*-----------------------------------------------------------*/

/* Each compilation unit must define the NetworkContext struct. */
struct NetworkContext {
  MbedTLSParams_t* pParams;
};

/*-----------------------------------------------------------*/

/**
 * @brief Log possible error from send/recv.
 *
 * @param[in] errorNumber Error number to be logged.
 */
static void logTransportError(int32_t errorNumber);

/*-----------------------------------------------------------*/

static void logTransportError(int32_t errorNumber) {
  /* Remove unused parameter warning. */
  (void)errorNumber;

  LogError(("A transport error occurred: %s.", strerror(errorNumber)));
}
/*-----------------------------------------------------------*/

void MbedTLS_initialize(NetworkContext_t* pNetworkContext) {
  MbedTLSParams_t* mbedTLSParams = pNetworkContext->pParams;
  mbedtls_net_init(&mbedTLSParams->server_fd);
  mbedtls_ssl_init(&mbedTLSParams->ssl);
  mbedtls_ssl_config_init(&mbedTLSParams->conf);
  mbedtls_ctr_drbg_init(&mbedTLSParams->ctr_drbg);
  mbedtls_entropy_init(&mbedTLSParams->entropy);

  const char* pers = "euphonium";
  int ret;
  if ((ret = mbedtls_ctr_drbg_seed(
           &mbedTLSParams->ctr_drbg, mbedtls_entropy_func,
           &mbedTLSParams->entropy, (const unsigned char*)pers,
           strlen(pers))) != 0) {
    LogError(("failed\n  ! mbedtls_ctr_drbg_seed returned "));
  }
}

SocketStatus_t MbedTLS_Connect(NetworkContext_t* pNetworkContext,
                               const ServerInfo_t* pServerInfo,
                               uint32_t sendTimeoutMs, uint32_t recvTimeoutMs) {
  SocketStatus_t returnStatus = SOCKETS_SUCCESS;
  MbedTLSParams_t* params = pNetworkContext->pParams;

  int portLength = snprintf(NULL, 0, "%d", pServerInfo->port);
  char* portStr = malloc(portLength + 1);
  snprintf(portStr, portLength + 1, "%d", pServerInfo->port);

  int ret;
  if ((ret = mbedtls_net_connect(&params->server_fd, pServerInfo->pHostName,
                                 portStr, MBEDTLS_NET_PROTO_TCP)) != 0) {
    returnStatus = SOCKETS_CONNECT_FAILURE;
  }

  if ((ret = mbedtls_ssl_config_defaults(&params->conf, MBEDTLS_SSL_IS_CLIENT,
                                         MBEDTLS_SSL_TRANSPORT_STREAM,
                                         MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
    returnStatus = SOCKETS_CONNECT_FAILURE;
  }

  mbedtls_ssl_conf_authmode(&params->conf, MBEDTLS_SSL_VERIFY_NONE);
  mbedtls_ssl_conf_rng(&params->conf, mbedtls_ctr_drbg_random,
                       &params->ctr_drbg);
  mbedtls_ssl_setup(&params->ssl, &params->conf);
  if ((ret = mbedtls_ssl_set_hostname(&params->ssl, "Mbed TLS Server 1")) !=
      0) {
    returnStatus = SOCKETS_CONNECT_FAILURE;
  }
  mbedtls_ssl_set_bio(&params->ssl, &params->server_fd, mbedtls_net_send,
                      mbedtls_net_recv, NULL);

  while ((ret = mbedtls_ssl_handshake(&params->ssl)) != 0) {
    if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
      returnStatus = SOCKETS_CONNECT_FAILURE;
    }
  }


  free(portStr);
  return returnStatus;
}
/*-----------------------------------------------------------*/

SocketStatus_t MbedTLS_Disconnect(const NetworkContext_t* pNetworkContext) {
  SocketStatus_t returnStatus = SOCKETS_SUCCESS;
  MbedTLSParams_t* params = NULL;

  /* Validate parameters. */
  if ((pNetworkContext == NULL) || (pNetworkContext->pParams == NULL)) {
    LogError(("Parameter check failed: pNetworkContext is NULL."));
    returnStatus = SOCKETS_INVALID_PARAMETER;
  } else {
    params = pNetworkContext->pParams;
    mbedtls_net_free(&params->server_fd);
    mbedtls_ssl_free(&params->ssl);
    mbedtls_ssl_config_free(&params->conf);
    mbedtls_ctr_drbg_free(&params->ctr_drbg);
    mbedtls_entropy_free(&params->entropy);
  }

  return returnStatus;
}
/*-----------------------------------------------------------*/

/* MISRA Rule 8.13 flags the following line for not using the const qualifier
 * on `pNetworkContext`. Indeed, the object pointed by it is not modified
 * by POSIX sockets, but other implementations of `TransportRecv_t` may do so. */
int32_t MbedTLS_Recv(NetworkContext_t* pNetworkContext, void* pBuffer,
                     size_t bytesToRecv) {
  MbedTLSParams_t* mbedTLSParams = NULL;
  int32_t bytesReceived = -1, pollStatus = 1;
  struct pollfd pollFds;

  assert(pNetworkContext != NULL && pNetworkContext->pParams != NULL);
  assert(pBuffer != NULL);
  assert(bytesToRecv > 0);

  /* Get receive timeout from the socket to use as the timeout for #select. */
  mbedTLSParams = pNetworkContext->pParams;

  /* Initialize the file descriptor.
     * #POLLPRI corresponds to high-priority data while #POLLIN corresponds
     * to any other data that may be read. */
  pollFds.events = POLLIN | POLLPRI;
  pollFds.revents = 0;
  /* Set the file descriptor for poll. */
  pollFds.fd = mbedTLSParams->server_fd.fd;

  /* Speculative read for the start of a payload.
     * Note: This is done to avoid blocking when
     * no data is available to be read from the socket. */
  if (bytesToRecv == 1U) {
    /* Check if there is data to read (without blocking) from the socket. */
    pollStatus = poll(&pollFds, 1, 0);
  }

  if (pollStatus > 0) {
    /* The socket is available for receiving data. */
    bytesReceived =
        (int32_t)mbedtls_ssl_read(&mbedTLSParams->ssl, pBuffer, bytesToRecv);
  } else if (pollStatus < 0) {
    /* An error occurred while polling. */
    bytesReceived = -1;
  } else {
    /* No data available to receive. */
    bytesReceived = 0;
  }

  /* Note: A zero value return from recv() represents
     * closure of TCP connection by the peer. */
  if ((pollStatus > 0) && (bytesReceived == 0)) {
    /* Peer has closed the connection. Treat as an error. */
    bytesReceived = -1;
  } else if (bytesReceived < 0) {
    logTransportError(errno);
  } else {
    /* Empty else MISRA 15.7 */
  }

  return bytesReceived;
}
/*-----------------------------------------------------------*/

/* MISRA Rule 8.13 flags the following line for not using the const qualifier
 * on `pNetworkContext`. Indeed, the object pointed by it is not modified
 * by POSIX sockets, but other implementations of `TransportSend_t` may do so. */
int32_t MbedTLS_Send(NetworkContext_t* pNetworkContext, const void* pBuffer,
                     size_t bytesToSend) {
  MbedTLSParams_t* mbedTLSParams = NULL;
  int32_t bytesSent = -1, pollStatus = -1;
  struct pollfd pollFds;

  assert(pNetworkContext != NULL && pNetworkContext->pParams != NULL);
  assert(pBuffer != NULL);
  assert(bytesToSend > 0);

  /* Get send timeout from the socket to use as the timeout for #select. */
  mbedTLSParams = pNetworkContext->pParams;

  /* Initialize the file descriptor. */
  pollFds.events = POLLOUT;
  pollFds.revents = 0;
  /* Set the file descriptor for poll. */
  pollFds.fd = mbedTLSParams->server_fd.fd;

  /* Check if data can be written to the socket.
     * Note: This is done to avoid blocking on send() when
     * the socket is not ready to accept more data for network
     * transmission (possibly due to a full TX buffer). */
  pollStatus = poll(&pollFds, 1, 0);

  if (pollStatus > 0) {
    /* The socket is available for sending data. */
    bytesSent =
        (int32_t)mbedtls_ssl_write(&mbedTLSParams->ssl, pBuffer, bytesToSend);
  } else if (pollStatus < 0) {
    /* An error occurred while polling. */
    bytesSent = -1;
  } else {
    /* Socket is not available for sending data. */
    bytesSent = 0;
  }

  if ((pollStatus > 0) && (bytesSent == 0)) {
    /* Peer has closed the connection. Treat as an error. */
    bytesSent = -1;
  } else if (bytesSent < 0) {
    logTransportError(errno);
  } else {
    /* Empty else MISRA 15.7 */
  }

  return bytesSent;
}
/*-----------------------------------------------------------*/