#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <fstream>
#include <chrono>

#include "quicftp_client.h"

int main(int argc, char *argv[]) {

 if(argc < 4) {
   std::cerr << "Usage: " << argv[0] << " <server> <upload|download> <file1> [file2 ...] [cert_path] [key_path]" << std::endl;
   std::cerr << "  server: e.g. 127.0.0.1:443" << std::endl;
   std::cerr << "  cert_path: (optional) path to client certificate" << std::endl;
   std::cerr << "  key_path: (optional) path to client key (if not part of cert)" << std::endl;
   return 1;
 }

 std::string server = argv[1];
 std::string mode = argv[2];
 std::vector<std::string> files;
 std::string cert_path = "";
 std::string key_path = "";

 // Simple argument parsing for trailing cert/key options
 // Files are in the middle
 for(int i=3; i<argc; i++) {
   std::string arg = argv[i];
   // Heuristic: if it looks like a cert or key file, treat it as such
   // This is a bit brittle but maintains partial compatibility with CLI style
   if (arg.find(".pem") != std::string::npos || arg.find(".crt") != std::string::npos || arg.find(".key") != std::string::npos) {
       if (cert_path.empty()) {
           cert_path = arg;
       } else if (key_path.empty()) {
           key_path = arg;
       }
   } else {
       files.push_back(arg);
   }
 }

 quicftp::Client client;
 client.set_verbose(true); // Enable verbose output for CLI

 if(!client.connect(server)) {
   std::cerr << "Connection setup failed" << std::endl;
   return 1;
 }

 if(!cert_path.empty()) {
    if(!client.authenticate(cert_path, key_path)) {
        std::cerr << "Authentication setup failed" << std::endl;
        return 1;
    }
 }

 if(mode == "upload") {
   if(files.size() == 1) {
     // Single file upload
     if(!client.upload_file(files[0], files[0])) {
       std::cerr << "Upload failed: " << files[0] << std::endl;
       return 1;
     }
   } else {
     // Multiple files - use parallel upload
     std::vector<std::pair<std::string, std::string>> file_pairs;
     for(const auto& file : files) {
       file_pairs.push_back({file, file}); // local and remote same for now
     }
     if(!client.upload_files(file_pairs)) {
       std::cerr << "Some uploads failed" << std::endl;
       return 1;
     }
   }

 } else if (mode == "download") {
   if(files.size() == 1) {
     // Single file download
     if(!client.download_file(files[0], files[0])) {
       std::cerr << "Download failed: " << files[0] << std::endl;
       return 1;
     }
   } else {
     // Multiple files - use parallel download
     std::vector<std::pair<std::string, std::string>> file_pairs;
     for(const auto& file : files) {
       file_pairs.push_back({file, file}); // remote and local same for now
     }
     if(!client.download_files(file_pairs)) {
       std::cerr << "Some downloads failed" << std::endl;
       return 1;
     }
   }
 } else {
     std::cerr << "Unknown mode: " << mode << std::endl;
     return 1;
 }

 // Client destructor handles cleanup (libcurl global cleanup)
 return 0;
}
