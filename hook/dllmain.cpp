
#pragma once
#define _WINSOCKAPI_

#include <stdio.h>
#include <Windows.h>
#include <WinSock2.h>
#include <ws2tcpip.h>


#pragma comment (lib, "Ws2_32.lib")

// __cdecl -> remember to clear stack after 
int socket_hook(int socket)
{
	FILE* fs = NULL;

	fopen_s(&fs, "socket.txt", "a");

	if (fs != NULL) {
		char buffer[32];
		int ret = sprintf_s(buffer, "Socket created: %d.\n", socket);
		fwrite(buffer, sizeof(char), strlen(buffer), fs);

		fclose(fs);
	}

	printf("Socket hook: %d\n", socket);

	return socket;
}

__declspec(naked) void socket_hook_wrapper()
{	// 								+ 4							+ 8
	// ESP ->[socket_lib_ret] [ socket_calle_ret(target) ] [ ARGS...]
	__asm {
		pop eax; // ESP -> [ socket_calle_ret(target)] [ ARGS...] 

		push [esp + 12]
		push [esp + 12]
		push [esp + 12]

		push process; //[process_addr][a3][a2][a1] [ socket_calle_ret(target)] [ ARGS...] 

		; execute stolen socket() instruction->prepare env
		sock:  
		  mov edi, edi
		  push ebp
		  mov ebp, esp
		  push ecx
		  push esi
		
		  jmp eax ; jump to the socket() -> let it to execute
					
		process: 
		  push eax; 
		  call socket_hook;
		  add esp, 4
		
          add esp, 16
		  jmp [esp - 16]
	}
}

void load_trampoline_socket()
{
	char buffer[] =
	{
		0xB8, 0xDE, 0xC0, 0xAD, 0xDE, // mov eax, 0xdeac0de
		0xFF, 0xD0					 // call eax 
	};

	void* wrapper_addr = &socket_hook_wrapper;

	memcpy(buffer + 1, &wrapper_addr, sizeof(void*));

	void* proc_address = GetProcAddress(GetModuleHandleA("ws2_32.dll"), "socket");

	if (proc_address == NULL) {
		puts("Error: GetProcAddress(socket).");
		return;
	}

	DWORD rights;
	if (!VirtualProtect(proc_address, sizeof(buffer), PAGE_EXECUTE_READWRITE, &rights)) {
		puts("Error: virtual protect(socket).");

		return;
	}

	memcpy(proc_address, buffer, sizeof(buffer));

	if (!VirtualProtect(proc_address, sizeof(buffer), rights, &rights)) {
		puts("Error: virtual protect restore(socket).");

		return;
	}
}

void listen_hook(int socket)
{
	FILE* fs = NULL;

	fopen_s(&fs, "listen.txt", "a");

	if (fs != NULL) {
		char buffer[32];
		int ret = sprintf_s(buffer, "Listen: %d.\n", socket);
		fwrite(buffer, sizeof(char), strlen(buffer), fs);

		fclose(fs);
	}

	printf("Listen hook: %d\n", socket);
}

__declspec(naked) void listen_hook_wrapper()
{
	// ESP -> [ listen_ret ] [ app_ret ] [ arg1] [arg2] 
	__asm {
		push[esp + 2 * 4]
		call listen_hook
		add esp, 4
		pop eax

		; restore original instructions before jump
		mov edi, edi
		push ebp
		mov ebp, esp
		push ecx


		//push eax 
		
		//push ecx 
		//mov ecx, 0x41AE7048
		//mov eax, 0x41AC2E29
		
		//mov eax, 0x774F2E29
		//cmp eax, DWORD ptr DS:[0x77517048]
		//mov eax, 0x2E
		//cmp eax, 0x774F2E29
		
		//cmp ecx, eax 

		//pop ecx 
		//pop eax 

		; restord 
		jmp eax
	};
}

int accept_hook(int socket, struct sockaddr* sockaddr)
{
	char ipaddress[INET6_ADDRSTRLEN];
	short port;
	void* addr = NULL;

	if (sockaddr->sa_family == AF_INET) {
		struct sockaddr_in* ipv4 = (struct sockaddr_in *)(sockaddr);
		addr = &(ipv4->sin_addr);
		port = ntohs(ipv4->sin_port);
	}

	inet_ntop((INT)sockaddr->sa_family, addr, ipaddress, sizeof(ipaddress));

	char msg_buffer[128];
	sprintf_s(msg_buffer, "Socket: %d, IP adddress : %s, Port: %d\n", socket, ipaddress, port);

	puts(msg_buffer);

	FILE* fd = NULL;
	fopen_s(&fd, "accept.txt", "a");

	if (fd != NULL) {
		fwrite(msg_buffer, sizeof(char), strlen(msg_buffer), fd);
	}
	fclose(fd);

	return socket;
}

void load_trampoline_listen()
{
	char buffer[] =
	{
		0xB8, 0xDE, 0xC0, 0xAD, 0xDE, // mov eax, 0xdeadc0de 
		0xFF, 0xD0,					  // call eax 
		0x90, 0x90, 0x90, 0x90,
		0x90, 0x90, 0x90, 0x90,
		0x90
	};
	
	void* wrapper_addr = &listen_hook_wrapper;
	memcpy(buffer + 1, &wrapper_addr, sizeof(void*));

	void* proc_address = GetProcAddress(GetModuleHandleA("ws2_32.dll"), "listen");

	if (proc_address == NULL) {
		puts("Error: GetProcAddress(listen).");

		return;
	}

	DWORD rights;
	if (!VirtualProtect(proc_address, sizeof(buffer), PAGE_EXECUTE_READWRITE, &rights)) {
		puts("Error: virtual protect(socket).");

		return;
	}

	memcpy(proc_address, buffer, sizeof(buffer));

	if (!VirtualProtect(proc_address, sizeof(buffer), rights, &rights)) {
		puts("Error: virtual protect restore(socket).");

		return;
	}
}

__declspec(naked) void accept_hook_wrapper()
{
	// ESP -> [ret_to_lib][ret_to_app][arg_1][arg_2][arg3]
	__asm {
		pop eax

		push[esp + 12]
		push[esp + 12]
		push[esp + 12]
		push process // ESP -> [process][arg_1][arg_2][arg_3][ret_to_app][arg_1][arg_2][arg3]

		; restore instructions
		mov edi, edi
		push ebp
		mov ebp, esp
		push 0

		jmp eax; jump back into accept 

		process:
		push [esp + 8] // sockaddr*
		push eax
		call accept_hook // ESP -> [ret][socket][sockaddr*][ret_to_app][arg_1][arg_2][arg3]
		add esp, 8; clear accept_hook_args // [ret_to_app][arg_1][arg_2][arg3]
		add esp, 16; clear old_args_for_accept // []
		jmp [esp - 16] // ESP -- > [ret_to_app]
	};
}
void load_trampoline_accept()
{
	char buffer[] =
	{
		0xB8, 0xDE, 0xC0, 0xAD, 0xDE, // mov eax, 0xdeac0de
		0xFF, 0xD0					 // call eax 
	};

	void* wrapper_addr = &accept_hook_wrapper;

	memcpy(buffer + 1, &wrapper_addr, sizeof(void*));

	void* proc_address = GetProcAddress(GetModuleHandleA("ws2_32.dll"), "accept");

	if (proc_address == NULL) {
		puts("Error: GetProcAddress(socket).");
		return;
	}

	DWORD rights;
	if (!VirtualProtect(proc_address, sizeof(buffer), PAGE_EXECUTE_READWRITE, &rights)) {
		puts("Error: virtual protect(accept).");

		return;
	}

	memcpy(proc_address, buffer, sizeof(buffer));

	if (!VirtualProtect(proc_address, sizeof(buffer), rights, &rights)) {
		puts("Error: virtual protect restore(accept).");

		return;
	}
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
	case DLL_PROCESS_ATTACH: {
		puts("Library loaded.");

		load_trampoline_socket();
		load_trampoline_listen();
		load_trampoline_accept();

		break;
	}

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

