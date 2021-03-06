
#pragma once
#define _WINSOCKAPI_
#include <stdio.h>
#include <Windows.h>
#include <WinSock2.h>
#include <ws2tcpip.h>

/*
	socket  -> V 
	accept  -> V 
	listen  -> V
	bind    -> V
	send    -> V 
	recv    -> V 
	connect -> 
*/

#pragma comment (lib, "Ws2_32.lib")


bool inject_code_into_target_procedure(void* proc, char* buffer, SIZE_T buffsize)
{
	DWORD tmp_rights = PAGE_EXECUTE_READWRITE;
	DWORD old_rights;

	if (!VirtualProtect(proc, buffsize, tmp_rights, &old_rights)) {
		return FALSE;
	}

	memcpy(proc, buffer, buffsize);

	if (!VirtualProtect(proc, buffsize, old_rights, &old_rights)) {
		return FALSE;
	}
}


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

	if (!inject_code_into_target_procedure(proc_address, buffer, sizeof(buffer))) {
		puts("Error: inject code into procedure(accept).");

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

	if (!inject_code_into_target_procedure(proc_address, buffer, sizeof(buffer))) {
		puts("Error: inject code into procedure(accept).");

		return;
	}
}

void bind_hook(int socket, struct sockaddr* saddr, int namelen)
{
	void* addr;
	char ipaddress[INET6_ADDRSTRLEN];

	if (saddr->sa_family = AF_INET) {
		struct sockaddr_in* ipv4 = (struct sockaddr_in *)saddr;
		addr = &(ipv4->sin_addr);
	}

	inet_ntop(saddr->sa_family, addr, ipaddress, INET6_ADDRSTRLEN);

	char msg_buffer[128];
	int ret = sprintf_s(msg_buffer, "Socket: %d, IP adddress : %s\n", socket, ipaddress);

	FILE* file = NULL;
	fopen_s(&file, "bind.txt", "a");

	if (file != NULL) {
		fwrite(msg_buffer, sizeof(char), ret + 1, file);
		fclose(file);
	}

	}
__declspec(naked) void bind_hook_wrapper()
{
	// ESP -> [ listen_ret ] [ app_ret ] [ arg1] [arg2] [arg3]
	__asm {
		push [esp + 16]
		push [esp + 16]
		push [esp + 16]
		call bind_hook
		add esp, 12
		pop eax
		// not good args 

		; restore original instructions before jump
		mov edi, edi
		push ebp
		mov ebp, esp
		push ecx

		; restord
		jmp eax // ESP -> [ app_ret ] [ arg1] [arg2] [arg3]
	};
}
void load_trampoline_bind()
{
	char buffer[] =
	{
		0xB8, 0xDE, 0xC0, 0xAD, 0xDE, // mov eax, 0xdeadc0de 
		0xFF, 0xD0,					  // call eax 
		0x90, 0x90, 0x90, 0x90,
		0x90, 0x90, 0x90, 0x90,
		0x90
	};

	void* wrapper_addr = &bind_hook_wrapper;
	memcpy(buffer + 1, &wrapper_addr, sizeof(void*));

	void* proc_address = GetProcAddress(GetModuleHandleA("ws2_32.dll"), "bind");

	if (proc_address == NULL) {
		puts("Error: GetProcAddress(listen).");

		return;
	}

	if (!inject_code_into_target_procedure(proc_address, buffer, sizeof(buffer))) {
		puts("Error: inject code into procedure(accept).");

		return;
	}
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

	FILE* fd = NULL;
	fopen_s(&fd, "accept.txt", "a");

	if (fd != NULL) {
		fwrite(msg_buffer, sizeof(char), strlen(msg_buffer), fd);
	}
	fclose(fd);

	return socket;
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

	if (!inject_code_into_target_procedure(proc_address, buffer, sizeof(buffer))) {
		puts("Error: inject code into procedure(accept).");

		return;
	}
}

int send_hook(int socket, int bytes_sent)
{
	FILE* fs = NULL;

	fopen_s(&fs, "send.txt", "a");

	if (fs != NULL) {
		char buffer[32];
		int ret = sprintf_s(buffer, "Socket: %d, bytes: %d.\n", socket, bytes_sent);
		fwrite(buffer, sizeof(char), strlen(buffer), fs);

		fclose(fs);
	}

	return bytes_sent;
}
__declspec(naked) void send_hook_wrapper() 
{
	// ESP -> [ret_lib][ret_app][arg_1][arg_2][arg_3][arg_4]
	__asm {
		pop eax; ESP ->[ret_app][arg_1][arg_2][arg_3][arg_4]

		push[esp + 16]
		push[esp + 16]
		push[esp + 16]
		push[esp + 16]

		push process 

		; restore original instructions before jump
		mov edi, edi
		push ebp
		mov ebp, esp
		sub esp, 0x10

		jmp eax
		; done 

		process: 
		push eax;      [bytes_sent][ret_app][arg_1][arg_2][arg_3][arg_4]
		push[esp + 8]; [socket][bytes_sent][ret_app][arg_1][arg_2][arg_3][arg_4]
		call send_hook
		add esp, 8 
		pop ecx 
		add esp, 16 
		jmp ecx
	}
}
void load_trampoline_send()
{
	char buffer[] =
	{
		0xB8, 0xDE, 0xC0, 0xAD, 0xDE, // mov eax, 0xDEADC0DE
		0xFF, 0xD0,					  // call eax 
		0x90						  // nop  
	};

	void* wrapper_addr = &send_hook_wrapper;

	memcpy(buffer + 1, &wrapper_addr, sizeof(void*));

	void* proc_address = GetProcAddress(GetModuleHandleA("ws2_32.dll"), "send");

	if (proc_address == NULL) {
		puts("Error: GetProcAddress(send).");

		return;
	}

	if (!inject_code_into_target_procedure(proc_address, buffer, sizeof(buffer))) {
		puts("Error: inject code into procedure(send).");

		return;
	}
}

int recv_hook(int socket, int bytes_recv)
{
	FILE* fs = NULL;

	fopen_s(&fs, "recv.txt", "a");

	if (fs != NULL) {
		char buffer[32];
		int ret = sprintf_s(buffer, "Socket: %d, bytes: %d.\n", socket, bytes_recv);
		fwrite(buffer, sizeof(char), strlen(buffer), fs);

		fclose(fs);
	}

	return bytes_recv;
}
__declspec(naked) void recv_hook_wrapper()
{
	// ESP -> [ret_lib][ret_app][arg_1][arg_2][arg_3][arg_4]
	__asm {
		pop eax

		push[esp + 16]
		push[esp + 16]
		push[esp + 16]
		push[esp + 16]

		push process

		; restore original instructions before jump
		mov edi, edi
		push ebp
		mov ebp, esp
		sub esp, 0x10

		jmp eax
		; done

		process: 
		push eax 
		push [esp + 8]
		call recv_hook
		add esp, 8
		pop ecx 
		add esp, 16 
		jmp ecx 
	}


}
void load_trampoline_recv()
{
	char buffer[] =
	{
		0xB8, 0xDE, 0xC0, 0xAD, 0xDE, // mov eax, 0xDEADC0DE
		0xFF, 0xD0,					  // call eax 
		0x90
	};

	void* wrapper_addr = &recv_hook_wrapper;

	memcpy(buffer + 1, &wrapper_addr, sizeof(void*));

	void* proc_address = GetProcAddress(GetModuleHandleA("ws2_32.dll"), "recv");

	if (proc_address == NULL) {
		puts("Error: GetProcAddress(recv).");

		return;
	}

	if (!inject_code_into_target_procedure(proc_address, buffer, sizeof(buffer))) {
		puts("Error: inject code into procedure(recv).");

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

		/*
			Scheme: 
			Step 1 - create trampoline, which is simple jump into our controlled function.
					The function must have apropriate address so example trampile may be defined as:
					mov eax, 0xDEADC0DE <- when loading library, this address hodul be replaced with &tramp_func
					jmp eax 

			Step 2 - in trampoline do what you need. For example process arguments for hooked function, or prepare 
			        env that the return address will be controlled by you. After execute stolen instructions. 

			Step 3 - jump into original code
		
		*/



		load_trampoline_socket();
		load_trampoline_listen();
		load_trampoline_accept();
		load_trampoline_send();
		load_trampoline_recv();
		load_trampoline_bind();

		break;
	}

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

