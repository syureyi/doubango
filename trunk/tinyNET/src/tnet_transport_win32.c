/*
* Copyright (C) 2009 Mamadou Diop.
*
* Contact: Mamadou Diop <diopmamadou@yahoo.fr>
*	
* This file is part of Open Source Doubango Framework.
*
* DOUBANGO is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*	
* DOUBANGO is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.
*	
* You should have received a copy of the GNU General Public License
* along with DOUBANGO.
*
*/

/**@file tnet_transport_32.c
 * @brief Network transport layer for WIN32 and WINCE systems.
 *
 * @author Mamadou Diop <diopmamadou(at)yahoo.fr>
 *
 * @date Created: Sat Nov 8 16:54:58 2009 mdiop
 */
#include "tnet_transport.h"
#include "tsk_memory.h"
#include "tsk_string.h"
#include "tsk_debug.h"
#include "tsk_thread.h"

#if TNET_UNDER_WINDOWS

/*== Socket description ==*/
typedef struct transport_socket_s
{
	WSABUF wsaBuffer;
	tnet_fd_t fd;
	unsigned connected:1;
}
transport_socket_t;

/*== Transport context structure definition ==*/
typedef struct transport_context_s
{
	size_t count;
	WSAEVENT events[WSA_MAXIMUM_WAIT_EVENTS];
	transport_socket_t* sockets[WSA_MAXIMUM_WAIT_EVENTS];
}
transport_context_t;

static void transport_socket_add(tnet_fd_t fd, transport_context_t *context);
static void transport_socket_remove(int index, transport_context_t *context);

/* 
* Add new socket to the watcher.
*/
int tnet_transport_add_socket(const tnet_transport_handle_t *handle, tnet_fd_t fd)
{
	tnet_transport_t *transport = (tnet_transport_t*)handle;
	transport_context_t *context;
	int ret = -1;

	if(!transport)
	{
		TSK_DEBUG_ERROR("Invalid server handle.");
		return ret;
	}

	context = (transport_context_t*)transport->context;
	transport_socket_add(fd, context);
	if(WSAEventSelect(fd, context->events[context->count - 1], FD_ALL_EVENTS) == SOCKET_ERROR)
	{
		transport_socket_remove((context->count - 1), context);
		TNET_PRINT_LAST_ERROR();
		return -1;
	}

	/* Signal */
	WSASetEvent(context->events[0]);

	return 0;
}

/*
* Connect stream/datagram socket to the specified destination. 
*/
tnet_fd_t tnet_transport_connectto(const tnet_transport_handle_t *handle, const char* host, tnet_port_t port)
{
	tnet_transport_t *transport = (tnet_transport_t*)handle;
	struct sockaddr_storage to;
	int status = -1;
	tnet_fd_t fd = INVALID_SOCKET;

	if(!transport)
	{
		TSK_DEBUG_ERROR("Invalid server handle.");
		goto bail;
	}

	/* Init destination sockaddr fields */
	if(status = tnet_sockaddr_init(host, port, transport->master->type, &to))
	{
		TSK_DEBUG_ERROR("Invalid HOST/PORT.");
		goto bail;
	}

	/*
	* STREAM ==> create new socket add connect it to the remote host.
	* DGRAM ==> connect the master to the remote host.
	*/
	if(TNET_SOCKET_TYPE_IS_STREAM(transport->master->type))
	{		
		/* Create socket descriptor. */
		if(status = tnet_sockfd_init(TNET_SOCKET_HOST_ANY, TNET_SOCKET_PORT_ANY, transport->master->type, &fd))
		{
			TSK_DEBUG_ERROR("Failed to create new sockfd.");
			
			goto bail;
		}

		/* Add the socket */
		if(status = tnet_transport_add_socket(handle, fd))
		{
			TNET_PRINT_LAST_ERROR();

			tnet_sockfd_close(&fd);
			goto bail;
		}
	}
	else
	{
		fd = transport->master->fd;
	}

	if((status = WSAConnect(fd, (LPSOCKADDR)&to, sizeof(to), NULL, NULL, NULL, NULL)) == SOCKET_ERROR)
	{
		if((status = WSAGetLastError()) == WSAEWOULDBLOCK)
		{
			TSK_DEBUG_INFO("WSAEWOULDBLOCK error for WSAConnect operation");
			status = 0;
		}
		else
		{
			TNET_PRINT_LAST_ERROR();

			tnet_sockfd_close(&fd);
			goto bail;
		}
	}
	
bail:
	return fd;
}

/*
* Sends stream/dgram data to the remote peer (previously connected to using @tnet_transport_connectto).
*/
size_t tnet_transport_send(const tnet_transport_handle_t *handle, tnet_fd_t from, const void* buf, size_t size)
{
	tnet_transport_t *transport = (tnet_transport_t*)handle;
	int ret = -1;
	WSABUF wsaBuffer;
	DWORD numberOfBytesSent = 0;

	if(!transport)
	{
		TSK_DEBUG_ERROR("Invalid transport handle.");
		goto bail;
	}

	wsaBuffer.buf = (CHAR*)buf;
	wsaBuffer.len = size;

	if((ret = WSASend(from, &wsaBuffer, 1, &numberOfBytesSent, 0, NULL, NULL)) == SOCKET_ERROR)
	{
		if((ret = WSAGetLastError()) == WSA_IO_PENDING)
		{
			TSK_DEBUG_INFO("WSA_IO_PENDING error for WSASend operation");
			ret = 0;
		}
		else
		{
			TNET_PRINT_LAST_ERROR();

			//tnet_sockfd_close(&from);
			goto bail;
		}
	}
	else ret = 0;

bail:
	return numberOfBytesSent;
}

/*
* Sends dgarm to the specified destionation.
*/
size_t tnet_transport_sendto(const tnet_transport_handle_t *handle, tnet_fd_t from, const struct sockaddr *to, const void* buf, size_t size)
{
	tnet_transport_t *transport = (tnet_transport_t*)handle;
	WSABUF wsaBuffer;
	DWORD numberOfBytesSent = 0;
	int ret = -1;
	
	if(!transport)
	{
		TSK_DEBUG_ERROR("Invalid server handle.");
		return ret;
	}
	
	if(!TNET_SOCKET_TYPE_IS_DGRAM(transport->master->type))
	{
		TSK_DEBUG_ERROR("In order to use WSASendTo you must use an udp transport.");
		return ret;
	}
	
	wsaBuffer.buf = (CHAR*)buf;
	wsaBuffer.len = size;
	
    if((ret = WSASendTo(from, &wsaBuffer, 1, &numberOfBytesSent, 0, to, sizeof(*to), 0, 0)) == SOCKET_ERROR)
	{
		if((ret = WSAGetLastError()) == WSA_IO_PENDING)
		{
			TSK_DEBUG_INFO("WSA_IO_PENDING error for WSASendTo operation");
			ret = 0;
		}
		else
		{
			TNET_PRINT_LAST_ERROR();
			return ret;
		}
	} else ret = 0;
		
	return numberOfBytesSent;
}








/*== CAllback function to check if we should accept the new socket or not == */
int CALLBACK AcceptCondFunc(LPWSABUF lpCallerId, LPWSABUF lpCallerData, LPQOS lpSQos, LPQOS lpGQos, LPWSABUF lpCalleeId, LPWSABUF lpCalleeData, GROUP FAR	*Group, DWORD CallbackData)
{
	transport_context_t *context = (transport_context_t*)CallbackData;
	return context->count < WSA_MAXIMUM_WAIT_EVENTS ? CF_ACCEPT : CF_REJECT;
}

/*== Add new socket ==*/
static void transport_socket_add(tnet_fd_t fd, transport_context_t *context)
{
	transport_socket_t *sock = tsk_calloc(1, sizeof(transport_socket_t));
	sock->fd = fd;
	
	context->events[context->count] = WSACreateEvent();
	context->sockets[context->count] = sock;

	context->count++;
}

/*== Remove socket ==*/
static void transport_socket_remove(int index, transport_context_t *context)
{
	int i;

	if(index < (int)context->count)
	{
		tnet_sockfd_close(&(context->sockets[index]->fd));
		if(context->sockets[index]->wsaBuffer.buf)
		{
			TSK_FREE(context->sockets[index]->wsaBuffer.buf);
		}
		TSK_FREE(context->sockets[index]);

		WSACloseEvent(context->events[index]);

		for(i=(context->count-1) ; i>index; i--)
		{
			context->sockets[i-1] = context->sockets[i];
			context->events[i-1] = context->events[i];
		}

		context->sockets[context->count-1] = 0;
		context->events[context->count-1] = 0;

		context->count--;
	}
}

/*=== stop all threads */
int tnet_transport_stop(tnet_transport_t *transport)
{	
	int ret;

	if(ret = tsk_runnable_stop(TSK_RUNNABLE(transport)))
	{
		return ret;
	}
	
	WSASetEvent(((transport_context_t*)(transport->context))->events[0]);
	return tsk_thread_join(transport->mainThreadId);
}

/*=== Main thread */
void *tnet_transport_mainthread(void *param)
{
	tnet_transport_t *transport = param;
	transport_context_t *context;
	DWORD evt;
	WSANETWORKEVENTS networkEvents;
	DWORD flags = 0;
	int ret;
	
	context = (transport_context_t*)tsk_calloc(1, sizeof(transport_context_t));
	transport->context = context;

	/* Start listening */
	if(TNET_SOCKET_TYPE_IS_STREAM(transport->master->type))
	{
		if(listen(transport->master->fd, WSA_MAXIMUM_WAIT_EVENTS))
		{
			TNET_PRINT_LAST_ERROR();
			goto bail;
		}
	}

	/* Add the current transport socket to the context. */
	transport_socket_add(transport->master->fd, context);
	if(ret = WSAEventSelect(transport->master->fd, context->events[context->count - 1], TNET_SOCKET_TYPE_IS_DGRAM(transport->master->type) ? FD_READ : FD_ALL_EVENTS/*FD_ACCEPT | FD_READ | FD_CONNECT | FD_CLOSE*/) == SOCKET_ERROR)
	{
		TNET_PRINT_LAST_ERROR();
		goto bail;
	}

	/* Set transport to active */
	transport->active = 1;

	TSK_DEBUG_INFO("Starting [%s] server with IP {%s} on port {%d}...", transport->description, transport->master->ip, transport->master->port);
	
	while(transport->running)
	{
		WSAEVENT active_event;
		transport_socket_t* active_socket;
		int index;

		/* Wait for multiple events */
		if((evt = WSAWaitForMultipleEvents(context->count, context->events, FALSE, WSA_INFINITE, FALSE)) == WSA_WAIT_FAILED)
		{
			TNET_PRINT_LAST_ERROR();
			goto bail;
		}

		if(!transport->running)
		{
			goto bail;
		}

		/* Get active event and socket */
		index = (evt - WSA_WAIT_EVENT_0);
		active_event = context->events[index];
		active_socket = context->sockets[index];

		/* Get the network events flags */
		if (WSAEnumNetworkEvents(active_socket->fd, active_event, &networkEvents) == SOCKET_ERROR)
		{
			TNET_PRINT_LAST_ERROR();
			goto bail;
		}

		/*================== FD_ACCEPT ==================*/
		if(networkEvents.lNetworkEvents & FD_ACCEPT)
		{
			tnet_fd_t fd;
			
			TSK_DEBUG_INFO("NETWORK EVENT FOR SERVER [%s] -- FD_ACCEPT", transport->description);

			if(networkEvents.iErrorCode[FD_ACCEPT_BIT])
			{
				TSK_DEBUG_ERROR("ACCEPT FAILED.");
				TNET_PRINT_LAST_ERROR();
				continue;
			}
			
			/* Accept the connection */
			if((fd = WSAAccept(active_socket->fd, NULL, NULL, AcceptCondFunc, (DWORD_PTR)context)) != INVALID_SOCKET)
			{
				/* Add the new fd to the server context */
				transport_socket_add(fd, context);
				if(WSAEventSelect(fd, context->events[context->count - 1], FD_READ | FD_WRITE | FD_CLOSE) == SOCKET_ERROR)
				{
					transport_socket_remove((context->count - 1), context);
					TNET_PRINT_LAST_ERROR();
					continue;
				}
			}
			else
			{
				TSK_DEBUG_ERROR("ACCEPT FAILED.");
				TNET_PRINT_LAST_ERROR();
				continue;
			}

			


		}

		/*================== FD_CONNECT ==================*/
		if(networkEvents.lNetworkEvents & FD_CONNECT)
		{
			//tnet_fd_t fd;

			TSK_DEBUG_INFO("NETWORK EVENT FOR SERVER [%s] -- FD_CONNECT", transport->description);

			if(networkEvents.iErrorCode[FD_CONNECT_BIT])
			{
				TSK_DEBUG_ERROR("CONNECT FAILED.");
				TNET_PRINT_LAST_ERROR();
				continue;
			}
			else
			{
				active_socket->connected = 1;
			}
		}


		/*================== FD_READ ==================*/
		if(networkEvents.lNetworkEvents & FD_READ)
		{
			DWORD readCount = 0;

			TSK_DEBUG_INFO("NETWORK EVENT FOR SERVER [%s] -- FD_READ", transport->description);

			if(networkEvents.iErrorCode[FD_READ_BIT])
			{
				TSK_DEBUG_ERROR("READ FAILED.");
				TNET_PRINT_LAST_ERROR();
				continue;
			}
			
			/* Create socket's internal buffer. */
			if(!active_socket->wsaBuffer.buf)
			{
				size_t max_buffer_size = TNET_SOCKET_TYPE_IS_DGRAM(transport->master->type) ? DGRAM_MAX_SIZE : STREAM_MAX_SIZE;
				active_socket->wsaBuffer.buf = tsk_calloc(max_buffer_size, sizeof(char));
				active_socket->wsaBuffer.len = max_buffer_size;
			}

			/* Receive the waiting data. */
			if(WSARecv(active_socket->fd, &(active_socket->wsaBuffer), 1, &readCount, &flags, 0, 0) == SOCKET_ERROR)
			{
				if(WSAGetLastError() == WSAEWOULDBLOCK)
				{
					TSK_DEBUG_INFO("WSAEWOULDBLOCK error for READ operation");
				}
				else
				{
					transport_socket_remove(index, context);
					TNET_PRINT_LAST_ERROR();
					continue;
				}
			}
			else
			{	
				TSK_RUNNABLE_ENQUEUE(TSK_RUNNABLE(transport), active_socket->wsaBuffer.buf, readCount);
			}
		}




		/*================== FD_WRITE ==================*/
		if(networkEvents.lNetworkEvents & FD_WRITE)
		{
			TSK_DEBUG_INFO("NETWORK EVENT FOR SERVER [%s] -- FD_WRITE", transport->description);

			if(networkEvents.iErrorCode[FD_WRITE_BIT])
			{
				TSK_DEBUG_ERROR("WRITE FAILED.");
				TNET_PRINT_LAST_ERROR();
				continue;
			}			
		}
		


		/*================== FD_CLOSE ==================*/
		if(networkEvents.lNetworkEvents & FD_CLOSE)
		{
			TSK_DEBUG_INFO("NETWORK EVENT FOR SERVER [%s] -- FD_CLOSE", transport->description);
		}

		/* Reset event */
		WSAResetEvent(active_event);

	} /* while(transport->running) */
	

bail:

	transport->active = 0;

	/* cleanup */
	while(context->count)
	{
		transport_socket_remove(0, context);
	}

	TSK_DEBUG_INFO("Stopping [%s] server with IP {%s} on port {%d}...", transport->description, transport->master->ip, transport->master->port);
	return 0;
}

#endif /* TNET_UNDER_WINDOWS */