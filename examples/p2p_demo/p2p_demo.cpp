#include <atomic>
#include <array>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#if defined(_WIN32)
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
constexpr socket_t kInvalidSocket = INVALID_SOCKET;
static int CloseSocket(socket_t socket) { return closesocket(socket); }
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
constexpr socket_t kInvalidSocket = -1;
static int CloseSocket(socket_t socket) { return close(socket); }
#endif

namespace {

constexpr int kDefaultPort = 27970;
constexpr int kBufferSize = 1024;

struct NetRuntime {
	NetRuntime() {
#if defined(_WIN32)
		WSADATA data{};
		ok = WSAStartup(MAKEWORD(2, 2), &data) == 0;
#else
		ok = true;
#endif
	}

	~NetRuntime() {
#if defined(_WIN32)
		if (ok) {
			WSACleanup();
		}
#endif
	}

	bool ok = false;
};

struct Endpoint {
	sockaddr_in addr{};

	std::string ToString() const {
		std::array<char, INET_ADDRSTRLEN> ip{};
		inet_ntop(AF_INET, &addr.sin_addr, ip.data(), static_cast<socklen_t>(ip.size()));
		return std::string(ip.data()) + ":" + std::to_string(ntohs(addr.sin_port));
	}
};

socket_t OpenUdpSocket(int port) {
	socket_t socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (socket == kInvalidSocket) {
		return kInvalidSocket;
	}

	sockaddr_in bindAddr{};
	bindAddr.sin_family = AF_INET;
	bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bindAddr.sin_port = htons(static_cast<uint16_t>(port));

	if (bind(socket, reinterpret_cast<const sockaddr*>(&bindAddr), sizeof(bindAddr)) != 0) {
		CloseSocket(socket);
		return kInvalidSocket;
	}

	return socket;
}

bool ParseEndpoint(std::string_view text, Endpoint& out) {
	const std::size_t colon = text.rfind(':');
	if (colon == std::string_view::npos || colon == 0 || colon + 1 >= text.size()) {
		return false;
	}

	const std::string host(text.substr(0, colon));
	const int port = std::stoi(std::string(text.substr(colon + 1)));
	if (port <= 0 || port > 65535) {
		return false;
	}

	out.addr = {};
	out.addr.sin_family = AF_INET;
	out.addr.sin_port = htons(static_cast<uint16_t>(port));
	return inet_pton(AF_INET, host.c_str(), &out.addr.sin_addr) == 1;
}

bool SendText(socket_t socket, const Endpoint& peer, std::string_view text) {
	const int sent = sendto(socket, text.data(), static_cast<int>(text.size()), 0,
		reinterpret_cast<const sockaddr*>(&peer.addr), sizeof(peer.addr));
	return sent == static_cast<int>(text.size());
}

std::optional<std::pair<std::string, Endpoint>> ReceiveText(socket_t socket, int timeoutMs) {
	fd_set readSet;
	FD_ZERO(&readSet);
	FD_SET(socket, &readSet);

	timeval timeout{};
	timeout.tv_sec = timeoutMs / 1000;
	timeout.tv_usec = (timeoutMs % 1000) * 1000;

	const int ready = select(static_cast<int>(socket + 1), &readSet, nullptr, nullptr, &timeout);
	if (ready <= 0) {
		return std::nullopt;
	}

	std::array<char, kBufferSize> buffer{};
	Endpoint from{};
	socklen_t fromLen = sizeof(from.addr);
	const int received = recvfrom(socket, buffer.data(), static_cast<int>(buffer.size() - 1), 0,
		reinterpret_cast<sockaddr*>(&from.addr), &fromLen);
	if (received <= 0) {
		return std::nullopt;
	}

	buffer[static_cast<std::size_t>(received)] = '\0';
	return std::make_pair(std::string(buffer.data()), from);
}

class Host {
public:
	bool Start(int port) {
		if (running_) {
			return true;
		}

		socket_ = OpenUdpSocket(port);
		if (socket_ == kInvalidSocket) {
			std::cerr << "Host failed to bind UDP port " << port << "\n";
			return false;
		}

		port_ = port;
		running_ = true;
		worker_ = std::thread([this]() { Run(); });
		std::cout << "[host] listening on 127.0.0.1:" << port_ << "\n";
		return true;
	}

	void Stop() {
		if (!running_) {
			return;
		}

		running_ = false;
		if (socket_ != kInvalidSocket && port_ > 0) {
			Endpoint self{};
			ParseEndpoint("127.0.0.1:" + std::to_string(port_), self);
			SendText(socket_, self, "quit");
		}
		if (worker_.joinable()) {
			worker_.join();
		}
		if (socket_ != kInvalidSocket) {
			CloseSocket(socket_);
			socket_ = kInvalidSocket;
		}
	}

	~Host() { Stop(); }

private:
	void Run() {
		while (running_) {
			auto packet = ReceiveText(socket_, 100);
			if (!packet) {
				continue;
			}

			const auto& [message, from] = *packet;
			lastPeer_ = from;
			std::cout << "[host] recv '" << message << "' from " << from.ToString() << "\n";

			if (message == "ping") {
				SendText(socket_, from, "pong");
				std::cout << "[host] sent 'pong'\n";
			} else {
				SendText(socket_, from, "echo:" + message);
			}
		}
	}

	socket_t socket_ = kInvalidSocket;
	int port_ = 0;
	std::atomic<bool> running_{false};
	std::thread worker_;
	std::optional<Endpoint> lastPeer_;
};

class Client {
public:
	bool Connect(std::string_view address) {
		Endpoint endpoint{};
		if (!ParseEndpoint(address, endpoint)) {
			std::cerr << "Client expected an IPv4 endpoint like 127.0.0.1:27970\n";
			return false;
		}

		socket_ = OpenUdpSocket(0);
		if (socket_ == kInvalidSocket) {
			std::cerr << "Client failed to open UDP socket\n";
			return false;
		}

		peer_ = endpoint;
		std::cout << "[client] peer set to " << peer_->ToString() << "\n";
		return true;
	}

	bool Ping(int timeoutMs = 1000) {
		if (socket_ == kInvalidSocket || !peer_) {
			std::cerr << "Client is not connected. Press button 2 first.\n";
			return false;
		}

		if (!SendText(socket_, *peer_, "ping")) {
			std::cerr << "[client] failed to send ping\n";
			return false;
		}

		std::cout << "[client] sent 'ping'\n";
		auto packet = ReceiveText(socket_, timeoutMs);
		if (!packet) {
			std::cerr << "[client] timeout waiting for pong\n";
			return false;
		}

		const auto& [message, from] = *packet;
		std::cout << "[client] recv '" << message << "' from " << from.ToString() << "\n";
		return message == "pong";
	}

	void Close() {
		if (socket_ != kInvalidSocket) {
			CloseSocket(socket_);
			socket_ = kInvalidSocket;
		}
		peer_.reset();
	}

	~Client() { Close(); }

private:
	socket_t socket_ = kInvalidSocket;
	std::optional<Endpoint> peer_;
};

bool RunSelfTest() {
	Host host;
	Client client;

	if (!host.Start(kDefaultPort)) {
		return false;
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	if (!client.Connect("127.0.0.1:" + std::to_string(kDefaultPort))) {
		return false;
	}

	const bool ok = client.Ping();
	host.Stop();

	std::cout << (ok ? "PROOF: ping -> pong succeeded\n" : "PROOF: ping -> pong failed\n");
	return ok;
}

void PrintMenu() {
	std::cout << "\n";
	std::cout << "+------------------------------+\n";
	std::cout << "| idtech3 tiny P2P demo        |\n";
	std::cout << "+------------------------------+\n";
	std::cout << "| [1] Start Host               |\n";
	std::cout << "| [2] Connect Client           |\n";
	std::cout << "| [3] Send Ping                |\n";
	std::cout << "| [4] Run Proof                |\n";
	std::cout << "| [q] Quit                     |\n";
	std::cout << "+------------------------------+\n";
	std::cout << "button> ";
}

int RunInteractive() {
	Host host;
	Client client;
	std::string input;

	for (;;) {
		PrintMenu();
		if (!std::getline(std::cin, input)) {
			break;
		}

		if (input == "1") {
			host.Start(kDefaultPort);
		} else if (input == "2") {
			client.Connect("127.0.0.1:" + std::to_string(kDefaultPort));
		} else if (input == "3") {
			std::cout << (client.Ping() ? "[proof] ping -> pong works\n" : "[proof] no pong\n");
		} else if (input == "4") {
			std::cout << (RunSelfTest() ? "[proof] local P2P loop works\n" : "[proof] local P2P loop failed\n");
		} else if (input == "q" || input == "Q") {
			break;
		}
	}

	return 0;
}

}  // namespace

int main(int argc, char** argv) {
	NetRuntime runtime;
	if (!runtime.ok) {
		std::cerr << "Network runtime initialization failed\n";
		return 1;
	}

	if (argc > 1 && std::string_view(argv[1]) == "--self-test") {
		return RunSelfTest() ? 0 : 1;
	}

	return RunInteractive();
}
