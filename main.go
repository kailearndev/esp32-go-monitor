package main

import (
	"fmt"
	"log"
	"os/exec"
	"runtime"
	"strings"
	"time"

	"github.com/shirou/gopsutil/v3/cpu"
	"github.com/shirou/gopsutil/v3/mem"
	"github.com/shirou/gopsutil/v3/process"
	"go.bug.st/serial"
)

// Lấy tiêu đề cửa sổ đang active (Hỗ trợ Win, Mac, Linux)
func getActiveWindowTitle() string {
	var out []byte
	var err error

	switch runtime.GOOS {
	case "windows":
		psCmd := `(Get-Process | Where-Object {$_.MainWindowHandle -eq (Get-ForegroundWindow)}).MainWindowTitle`
		out, err = exec.Command("powershell", "-Command", psCmd).Output()
	case "darwin": // macOS
		asCmd := `tell application "System Events" to get name of window 1 of (first process whose frontmost is true)`
		out, err = exec.Command("osascript", "-e", asCmd).Output()
	case "linux": // Cần cài 'xdotool' trên Linux
		out, err = exec.Command("xdotool", "getactivewindow", "getwindowname").Output()
	}

	if err != nil {
		return ""
	}
	return strings.ToLower(strings.TrimSpace(string(out)))
}

func main() {
	// Cổng COM cho Windows hoặc tty cho Mac/Linux
	portName := "COM3"
	if runtime.GOOS != "windows" {
		portName = "/dev/tty.usbserial-110" // Ví dụ, ông thay bằng cổng thực tế
	}

	mode := &serial.Mode{BaudRate: 115200}
	port, err := serial.Open(portName, mode)
	if err != nil {
		log.Fatalf("❌ Lỗi cổng %s: %v", portName, err)
	}
	defer port.Close()

	fmt.Printf("🚀 LEO AI (%s) - Hệ thống đã sẵn sàng!\n", runtime.GOOS)
	lastActivityTime := time.Now()

	for {
		v, _ := mem.VirtualMemory()
		c, _ := cpu.Percent(0, false)
		cpuUsage := 0.0
		if len(c) > 0 {
			cpuUsage = c[0]
		}

		isCoding, activeTech := false, "NONE"
		title := getActiveWindowTitle()

		// Quét tiến trình để đảm bảo độ nhạy (Dùng cho cả 3 OS)
		procs, _ := process.Processes()
		for _, p := range procs {
			name, _ := p.Name()
			if strings.Contains(strings.ToLower(name), "code") {
				isCoding = true
				lastActivityTime = time.Now()
				// Detect ngôn ngữ qua Title
				if strings.Contains(title, ".go") {
					activeTech = "GOLANG"
				} else if strings.Contains(title, ".tsx") || strings.Contains(title, ".jsx") {
					activeTech = "NEXT/REACT"
				} else {
					activeTech = "VSCODE"
				}
				break
			}
		}

		newState := "IDLE"
		idleDuration := time.Since(lastActivityTime)
		if isCoding {
			newState = "THINKING"
		} else if idleDuration > 2*time.Minute {
			newState = "NIGHT"
		}

		// Lời chào theo buổi
		hour := time.Now().Hour()
		greeting := "HELLO, KAI"
		if hour < 12 {
			greeting = "GOOD MORNING"
		} else if hour < 18 {
			greeting = "GOOD AFTERNOON"
		} else {
			greeting = "GOOD EVENING"
		}

		datetime := time.Now().Format("15:04:05 | 02/01")
		msg := fmt.Sprintf("STATE:%s|%d|%d|45.0|%s|%s|%s|0\n", newState, int(cpuUsage), int(v.UsedPercent), greeting, activeTech, datetime)
		port.Write([]byte(msg))

		fmt.Printf("\r🕒 %s | 🤖 %-8s | Tech: %-10s", datetime, newState, activeTech)
		time.Sleep(1 * time.Second)
	}
}
