BINARY_NAME=monitor
BUILD_DIR=bin
LDFLAGS=-ldflags="-s -w"

.PHONY: all build-win build-mac build-linux clean

all: clean build-win build-mac build-linux

build-win:
	@echo "Building for Windows..."
	go build $(LDFLAGS) -o $(BUILD_DIR)/$(BINARY_NAME)_win.exe main.go

build-mac:
	@echo "Building for macOS..."
	go build $(LDFLAGS) -o $(BUILD_DIR)/$(BINARY_NAME)_mac main.go

build-linux:
	@echo "Building for Linux..."
	go build $(LDFLAGS) -o $(BUILD_DIR)/$(BINARY_NAME)_linux main.go

# SỬA LẠI ĐOẠN NÀY: Dùng Go để xóa thư mục cho Windows nó hiểu
clean:
	@echo "Cleaning up..."
	@go run -e "import \"os\"; os.RemoveAll(\"$(BUILD_DIR)\")" || exit 0