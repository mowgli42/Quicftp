#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <fstream>
#include <chrono>
#include <cstring>

#include "quicftp_client.h"

static void print_usage(const char* program) {
    std::cerr << "Usage: " << program << " [options] <server> <upload|download> <file1> [file2 ...]" << std::endl;
    std::cerr << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "  --help              Show this help message" << std::endl;
    std::cerr << "  --version           Show version" << std::endl;
    std::cerr << "  --verbose           Enable verbose output" << std::endl;
    std::cerr << "  --insecure          Disable TLS certificate verification" << std::endl;
    std::cerr << "  --ca-cert <path>    Path to CA certificate for TLS verification" << std::endl;
    std::cerr << "  --cert <path>       Path to client certificate" << std::endl;
    std::cerr << "  --key <path>        Path to client private key" << std::endl;
    std::cerr << "  --progress          Show transfer progress" << std::endl;
    std::cerr << std::endl;
    std::cerr << "Examples:" << std::endl;
    std::cerr << "  " << program << " localhost:443 upload file.txt" << std::endl;
    std::cerr << "  " << program << " localhost:443 download file.txt" << std::endl;
    std::cerr << "  " << program << " localhost:443 upload a.txt b.txt c.txt" << std::endl;
    std::cerr << "  " << program << " --cert cert.pem --key key.pem localhost:443 upload file.txt" << std::endl;
}

static void print_version() {
    std::cout << "quicftpclient 0.2.0" << std::endl;
}

static void progress_callback(const std::string& file, size_t transferred, size_t total) {
    if (total > 0) {
        int pct = static_cast<int>((transferred * 100) / total);
        std::cerr << "\r  " << file << ": " << pct << "% (" 
                  << transferred << "/" << total << " bytes)" << std::flush;
        if (transferred >= total) {
            std::cerr << std::endl;
        }
    }
}

int main(int argc, char *argv[]) {
    // Parse arguments
    std::string server;
    std::string mode;
    std::vector<std::string> files;
    std::string cert_path;
    std::string key_path;
    std::string ca_cert_path;
    bool verbose = false;
    bool insecure = false;
    bool show_progress = false;
    bool insecure_set = false;

    int i = 1;
    while (i < argc) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--version") {
            print_version();
            return 0;
        } else if (arg == "--verbose" || arg == "-v") {
            verbose = true;
        } else if (arg == "--insecure" || arg == "-k") {
            insecure = true;
            insecure_set = true;
        } else if (arg == "--progress") {
            show_progress = true;
        } else if ((arg == "--ca-cert") && i + 1 < argc) {
            ca_cert_path = argv[++i];
        } else if ((arg == "--cert") && i + 1 < argc) {
            cert_path = argv[++i];
        } else if ((arg == "--key") && i + 1 < argc) {
            key_path = argv[++i];
        } else if (arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        } else {
            // Positional arguments: server, mode, files
            if (server.empty()) {
                server = arg;
            } else if (mode.empty()) {
                mode = arg;
            } else {
                files.push_back(arg);
            }
        }
        i++;
    }

    if (server.empty() || mode.empty() || files.empty()) {
        print_usage(argv[0]);
        return 1;
    }

    if (mode != "upload" && mode != "download") {
        std::cerr << "Error: Unknown mode '" << mode << "'. Use 'upload' or 'download'." << std::endl;
        return 1;
    }

    // Configure client
    quicftp::Client client;
    client.set_verbose(verbose);

    // Default to insecure for backward compatibility (unless CA cert provided)
    if (!ca_cert_path.empty()) {
        client.set_ca_cert(ca_cert_path);
    } else if (insecure_set) {
        client.set_insecure(insecure);
    } else {
        client.set_insecure(true); // Default insecure for now
    }

    if (show_progress) {
        client.set_progress_callback(progress_callback);
    }

    if (!client.connect(server)) {
        std::cerr << "Error: Connection setup failed" << std::endl;
        return 1;
    }

    if (!cert_path.empty()) {
        if (!client.authenticate(cert_path, key_path)) {
            std::cerr << "Error: Authentication setup failed" << std::endl;
            return 1;
        }
    }

    // Execute transfers
    int exit_code = 0;

    if (mode == "upload") {
        if (files.size() == 1) {
            auto result = client.upload(files[0], files[0]);
            if (result.success) {
                std::cout << "Uploaded " << files[0] << " (" << result.bytes_transferred << " bytes)" << std::endl;
            } else {
                std::cerr << "Error: " << result.message;
                if (result.http_status > 0) {
                    std::cerr << " (HTTP " << result.http_status << ")";
                }
                std::cerr << std::endl;
                exit_code = 1;
            }
        } else {
            std::vector<std::pair<std::string, std::string>> file_pairs;
            for (const auto& file : files) {
                file_pairs.push_back({file, file});
            }
            auto batch = client.upload_batch(file_pairs);
            for (const auto& [path, result] : batch.results) {
                if (result.success) {
                    std::cout << "Uploaded " << path << " (" << result.bytes_transferred << " bytes)" << std::endl;
                } else {
                    std::cerr << "Error [" << path << "]: " << result.message << std::endl;
                }
            }
            std::cout << batch.succeeded_count << "/" << batch.results.size() << " files uploaded successfully" << std::endl;
            if (!batch.all_success) exit_code = 1;
        }

    } else { // download
        if (files.size() == 1) {
            auto result = client.download(files[0], files[0]);
            if (result.success) {
                std::cout << "Downloaded " << files[0] << " (" << result.bytes_transferred << " bytes)" << std::endl;
            } else {
                std::cerr << "Error: " << result.message;
                if (result.http_status > 0) {
                    std::cerr << " (HTTP " << result.http_status << ")";
                }
                std::cerr << std::endl;
                exit_code = 1;
            }
        } else {
            std::vector<std::pair<std::string, std::string>> file_pairs;
            for (const auto& file : files) {
                file_pairs.push_back({file, file});
            }
            auto batch = client.download_batch(file_pairs);
            for (const auto& [path, result] : batch.results) {
                if (result.success) {
                    std::cout << "Downloaded " << path << " (" << result.bytes_transferred << " bytes)" << std::endl;
                } else {
                    std::cerr << "Error [" << path << "]: " << result.message << std::endl;
                }
            }
            std::cout << batch.succeeded_count << "/" << batch.results.size() << " files downloaded successfully" << std::endl;
            if (!batch.all_success) exit_code = 1;
        }
    }

    return exit_code;
}
