#include <iostream>
#include <string>
#include <vector>

#include "quicftp_client.h"

int main(int argc, char *argv[]) {

 if(argc < 4) {
   std::cerr << "Usage: " << argv[0] << " <server> <upload|download> <file1> <file2> ..." << std::endl;
   return 1;
 }

 std::string server = argv[1];
 std::string mode = argv[2];
 std::vector<std::string> files;

 for(int i=3; i<argc; i++) {
   files.push_back(argv[i]);
 }

 quicftp::Client client;

 if(!client.connect(server)) {
   std::cerr << "Connection failed" << std::endl;
   return 1;
 }

 // Use certificate based auth
 if(!client.authenticate("/path/to/cert.pem")) {
   std::cerr << "Authentication failed" << std::endl;
   return 1;
 }

 if(mode == "upload") {
   for(const auto& file : files) {
     if(!client.upload_file(file)) {
       std::cerr << "Upload failed: " << file << std::endl;
     }
   }

 } else if (mode == "download") {
   for(const auto& file : files) {
     if(!client.download_file(file)) {
       std::cerr << "Download failed: " << file << std::endl;
     }
   }
 }

 client.disconnect();

 return 0;
}