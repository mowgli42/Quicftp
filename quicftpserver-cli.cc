#include <iostream>
#include <string>
#include <csignal>
#include <cstdlib>
#include <thread>
#include <chrono>

#include "quicftp_server.h"

static quicftp::Server* g_server = nullptr;

void signal_handler(int signal) {
  if (g_server != nullptr) {
    std::cerr << "\nReceived signal " << signal << ", shutting down server..." << std::endl;
    g_server->stop();
    std::exit(0);
  }
}

void print_usage(const char* program_name) {
  std::cerr << "Usage: " << program_name 
            << " <port> <cert_path> <key_path> [root_dir] [--quiet]" << std::endl;
  std::cerr << std::endl;
  std::cerr << "Arguments:" << std::endl;
  std::cerr << "  port       - Port number to listen on" << std::endl;
  std::cerr << "  cert_path  - Path to server certificate file" << std::endl;
  std::cerr << "  key_path   - Path to server private key file" << std::endl;
  std::cerr << "  root_dir   - Root directory for file storage (default: current directory)" << std::endl;
  std::cerr << "  --quiet    - Disable verbose logging" << std::endl;
  std::cerr << std::endl;
  std::cerr << "Example:" << std::endl;
  std::cerr << "  " << program_name << " 4433 server.crt server.key /var/quicftp" << std::endl;
}

int main(int argc, char *argv[]) {
  if (argc < 4) {
    print_usage(argv[0]);
    return 1;
  }

  // Parse arguments
  int port = std::stoi(argv[1]);
  std::string cert_path = argv[2];
  std::string key_path = argv[3];
  std::string root_dir = ".";
  bool verbose = true;

  // Parse optional arguments
  for (int i = 4; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--quiet") {
      verbose = false;
    } else if (root_dir == "." && arg[0] != '-') {
      // First non-flag argument after required args is root_dir
      root_dir = arg;
    }
  }

  // Validate port
  if (port < 1 || port > 65535) {
    std::cerr << "Error: Port must be between 1 and 65535" << std::endl;
    return 1;
  }

  // Create and configure server
  quicftp::Server server;
  g_server = &server; // For signal handler

  server.set_verbose(verbose);
  server.set_root_directory(root_dir);

  // Set up signal handlers for graceful shutdown
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  // Start server
  if (!server.start(port, cert_path, key_path, root_dir)) {
    std::cerr << "Failed to start server" << std::endl;
    return 1;
  }

  // Display startup information
  std::cout << std::endl;
  std::cout << "=== Quicftp Test Server ===" << std::endl;
  std::cout << "Port: " << port << std::endl;
  std::cout << "Certificate: " << cert_path << std::endl;
  std::cout << "Key: " << key_path << std::endl;
  std::cout << "Root directory: " << server.get_root_directory() << std::endl;
  std::cout << "Verbose logging: " << (verbose ? "enabled" : "disabled") << std::endl;
  std::cout << std::endl;
  std::cout << "Server is running. Press Ctrl+C to stop." << std::endl;
  std::cout << std::endl;

  // Main event loop - process QUIC events
  while (server.is_running()) {
    server.process_events(100); // Process events with 100ms timeout
  }

  return 0;
}

