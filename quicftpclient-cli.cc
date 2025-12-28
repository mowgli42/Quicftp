#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <fstream>
#include <chrono>

#include "quicftp_client.h"

int main(int argc, char *argv[]) {

 if(argc < 4) {
   std::cerr << "Usage: " << argv[0] << " <server> <upload|download> <file1> [file2 ...] [cert_path]" << std::endl;
   std::cerr << "  cert_path is optional (if ends with .pem/.crt or contains 'cert'), defaults to certs/client-cert.pem" << std::endl;
   return 1;
 }

 std::string server = argv[1];
 std::string mode = argv[2];
 std::vector<std::string> files;
 std::string cert_path = "certs/client-cert.pem"; // Default

 // Parse arguments: files and optional cert path
 // If last arg looks like a cert path (ends with .pem or contains "cert"), use it as cert_path
 // Otherwise, all args from index 3 are files
 for(int i=3; i<argc; i++) {
   std::string arg = argv[i];
   // Check if this looks like a certificate path
   bool is_cert = (arg.find("cert") != std::string::npos) ||
                  (arg.length() >= 4 && arg.substr(arg.length() - 4) == ".pem") ||
                  (arg.length() >= 4 && arg.substr(arg.length() - 4) == ".crt");
   if (is_cert) {
     cert_path = arg;
   } else {
     files.push_back(arg);
   }
 }

 // #region agent log
 {
   std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
   if (log_file.is_open()) {
     log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A\",\"location\":\"quicftpclient-cli.cc:33\",\"message\":\"Parsed certificate path\",\"data\":{\"cert_path\":\"" << cert_path << "\",\"argc\":" << argc << "},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
     log_file.close();
   }
 }
 // #endregion

 quicftp::Client client;

 if(!client.connect(server)) {
   std::cerr << "Connection failed" << std::endl;
   return 1;
 }

 // Use certificate based auth
 // #region agent log
 {
   std::ofstream log_file("/home/tprettol/repo/Quicftp/.cursor/debug.log", std::ios::app);
   if (log_file.is_open()) {
     log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A\",\"location\":\"quicftpclient-cli.cc:50\",\"message\":\"About to call authenticate()\",\"data\":{\"cert_path\":\"" << cert_path << "\"},\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
     log_file.close();
   }
 }
 // #endregion
 if(!client.authenticate(cert_path)) {
   std::cerr << "Authentication failed" << std::endl;
   return 1;
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
 }

 client.disconnect();

 return 0;
}