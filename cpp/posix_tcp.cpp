
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <thread>
#include <future>
#include <vector>

#include <fibre/fibre.hpp>

#define TCP_RX_BUF_LEN	512

namespace fibre {

class TCPConnection : public OutputChannel, public StreamSource {
public:
    TCPConnection(int socket_fd) :
        socket_fd_(socket_fd)
    {
        int val = 0;
        socklen_t val_len = sizeof(val);
        if (getsockopt(socket_fd_, SOL_SOCKET, SO_SNDBUF, &val, &val_len) != 0) {
            LOG_FIBRE("failed to get socket send buffer size");
            throw "TCP connection init failed";
        }
        if (val < 0) {
            LOG_FIBRE("invalid socket send buffer size");
            throw "TCP connection init failed";
        }
        kernel_send_buffer_size_ = val;
    }

    ~TCPConnection() {
        close(socket_fd_);
    }

    StreamSink::status_t process_bytes(const uint8_t* buffer, size_t length, size_t* processed_bytes) final {
        int bytes_sent = send(socket_fd_, buffer, length, 0);
        if (processed_bytes)
            *processed_bytes = (bytes_sent == -1) ? 0 : bytes_sent;
        return (bytes_sent == -1) ? StreamSink::ERROR : StreamSink::OK;
    }

    size_t get_min_non_blocking_bytes() const final {
        int pending_bytes = 0;
        ioctl(socket_fd_, TIOCOUTQ, &pending_bytes);
        if (pending_bytes < 0) {
            LOG_FIBRE("less than zero pending bytes");
            return kernel_send_buffer_size_;
        } else if (static_cast<unsigned int>(pending_bytes) > kernel_send_buffer_size_) {
            LOG_FIBRE("a lot of pending bytes");
            return 0;
        } else {
            return kernel_send_buffer_size_ - pending_bytes;
        }
    }

    StreamSource::status_t get_bytes(uint8_t* buffer, size_t length, int flags, size_t* generated_bytes) {
        // returns as soon as there is some data
        // -1 indicates error and 0 means that the client gracefully terminated
        ssize_t n_received_signed = recv(socket_fd_, buffer, length, flags);

        if (n_received_signed == 0) {
            LOG_FIBRE("TCP connection closed by remote host");
            return StreamSource::CLOSED;
        }

        if (n_received_signed < 0) {
            LOG_FIBRE("TCP connection broke unexpectedly: %s", strerror(errno));
            return StreamSource::ERROR;
        }

        size_t n_received = static_cast<size_t>(n_received_signed);
        if (n_received > length) {
            LOG_FIBRE("too many bytes received");
            return StreamSource::ERROR;
        }

        if (generated_bytes)
            *generated_bytes += n_received;
        return StreamSource::OK;
    }

    StreamSource::status_t get_bytes(uint8_t* buffer, size_t min_length, size_t max_length, size_t* generated_bytes) final {
        if (min_length > max_length) {
            return StreamSource::ERROR;
        }

        StreamSource::status_t status;
        if (min_length > 1) {
            // TODO: set timeout
            size_t chunk = 0;
            status = get_bytes(buffer, min_length, MSG_WAITALL, &chunk);
            if (status != StreamSource::OK)
                return status;
            status = get_bytes(buffer + chunk, max_length - chunk, MSG_DONTWAIT, &chunk);
            if (generated_bytes)
                *generated_bytes += chunk;
            return status;
        } else {
            return get_bytes(buffer, max_length, 0, generated_bytes);
        }
    }

    void terminate() {
        shutdown(socket_fd_, SHUT_RDWR);
    }

private:
    int socket_fd_;
    size_t kernel_send_buffer_size_;
};

bool serve_client(int socket_fd) {
    TCPConnection connection(socket_fd);
    uint8_t uuid_buf[16];

    LOG_FIBRE("sending own UUID");
    if (connection.process_bytes(global_state.own_uuid.get_bytes().data(), 16, nullptr) != StreamSink::OK) {
        LOG_FIBRE("failed to send own UUID");
        return false;
    }

    LOG_FIBRE("waiting for remote UUID");
    if (connection.get_bytes(uuid_buf, 16, 16, nullptr) != StreamSource::OK) {
        LOG_FIBRE("failed to get remote UUID");
        return false;
    }

    LOG_FIBRE("handshake complete");

    RemoteNode* remote_node = fibre::get_remote_node(Uuid(uuid_buf));
    remote_node->add_output_channel(&connection);
    InputChannelDecoder input_decoder(remote_node);

    for (;;) {
        uint8_t buf[TCP_RX_BUF_LEN];
        size_t n_received = 0;
        StreamSource::status_t status =
                connection.get_bytes(buf, 1, sizeof(buf), &n_received);
        if (status != StreamSource::OK) {
            LOG_FIBRE("connection closed");
            break;
        }
        input_decoder.process_bytes(buf, n_received, nullptr);
    }

    if (remote_node)
        remote_node->remove_output_channel(&connection);

    return 0;
}


// function to check if a worker thread handling a single client is done
template<typename T>
bool future_is_ready(std::future<T>& t){
    return t.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

int serve_on_tcp(unsigned int port) {
    struct sockaddr_in6 si_me, si_other;
    int s;


    if ((s=socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        LOG_FIBRE("failed to create socket");
        return -1;
    }

    memset((char *) &si_me, 0, sizeof(si_me));
    si_me.sin6_family = AF_INET6;
    si_me.sin6_port = htons(port);
    si_me.sin6_flowinfo = 0;
    si_me.sin6_addr = in6addr_any;
    if (bind(s, reinterpret_cast<struct sockaddr *>(&si_me), sizeof(si_me)) == -1) {
        LOG_FIBRE("failed to bind socket");
        return -1;
    }

    // make this socket a passive socket
    if (listen(s, 128) != 0) {
        LOG_FIBRE("failed to listen on TCP");
        return -1;
    }

    LOG_FIBRE("listening");
    std::vector<std::future<bool>> serv_pool;
    for (;;) {
        memset(&si_other, 0, sizeof(si_other));

        socklen_t silen = sizeof(si_other);
        // TODO: Add a limit on accepting connections
        int client_portal_fd = accept(s, reinterpret_cast<sockaddr *>(&si_other), &silen); // blocking call
        LOG_FIBRE("accepted connection");
        serv_pool.push_back(std::async(std::launch::async, serve_client, client_portal_fd));
        // do a little clean up on the pool
        for (std::vector<std::future<bool>>::iterator it = serv_pool.end()-1; it >= serv_pool.begin(); --it) {
            if (future_is_ready(*it)) {
                // we can erase this thread
                serv_pool.erase(it);
            }
        }
    }

    close(s);
}

}
