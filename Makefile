# Makefile for Quicftp
# Fallback build system if CMake is not available

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
LDFLAGS = -lssl -lcrypto -lpthread

# Directories
SRCDIR = .
OBJDIR = obj
BINDIR = bin

# Source files
CLIENT_SRC = quicftp_client.cc quicftpclient-cli.cc
SERVER_SRC = quicftp_server.cc quicftpserver-cli.cc

# Object files
CLIENT_OBJ = $(CLIENT_SRC:%.cc=$(OBJDIR)/%.o)
SERVER_OBJ = $(SERVER_SRC:%.cc=$(OBJDIR)/%.o)

# Targets
all: $(BINDIR)/quicftpclient $(BINDIR)/quicftpserver

$(BINDIR)/quicftpclient: $(CLIENT_OBJ) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BINDIR)/quicftpserver: $(SERVER_OBJ) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cc | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJDIR) $(BINDIR):
	mkdir -p $@

clean:
	rm -rf $(OBJDIR) $(BINDIR)

.PHONY: all clean

