
#define FIBRE_ENABLE_SERVER F_RUNTIME_CONFIG
#define FIBRE_ENABLE_CLIENT F_RUNTIME_CONFIG
#define FIBRE_ENABLE_EVENT_LOOP 1

#if defined(__linux__)
#define FIBRE_ENABLE_TCP_CLIENT_BACKEND 1
#define FIBRE_ENABLE_TCP_SERVER_BACKEND 1
#define FIBRE_ENABLE_SOCKET_CAN_BACKEND 1
#endif

#if FIBRE_ENABLE_SOCKET_CAN_BACKEND
#define FIBRE_ENABLE_CAN_ADAPTER 1
#endif