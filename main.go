package main

import (
	"fmt"
	"log"
	"strings"
	"time"

	"github.com/shirou/gopsutil/v3/cpu"
	"github.com/shirou/gopsutil/v3/host"
	"github.com/shirou/gopsutil/v3/mem"
	"github.com/shirou/gopsutil/v3/process"
	"go.bug.st/serial"
)

func main() {
	portName := "COM3" // Nhớ check cổng COM
	mode := &serial.Mode{BaudRate: 115200}
	port, err := serial.Open(portName, mode)
	if err != nil {
		log.Fatalf("❌ Lỗi cổng %s: %v", portName, err)
	}
	defer port.Close()

	fmt.Println("✅ LEO Monitor đang chạy! Đang quét hệ thống...")

	for {
		v, _ := mem.VirtualMemory()
		ramUsage := v.UsedPercent

		c, _ := cpu.Percent(0, false)
		cpuUsage := 0.0
		if len(c) > 0 {
			cpuUsage = c[0]
		}

		sensors, _ := host.SensorsTemperatures()
		temp := 0.0
		if len(sensors) > 0 {
			temp = sensors[0].Temperature
		} else {
			temp = 40.0 + (cpuUsage * 0.35) // Fake temp nếu bị Windows khóa
		}

		isCoding := false
		isGit := false
		activeTech := "NONE"

		// Quét tiến trình xem có đang Code hoặc chạy Git không
		procs, err := process.Processes()
		if err == nil {
			for _, p := range procs {
				name, _ := p.Name()
				nameLower := strings.ToLower(name)

				// Detect Git (Pull, Push, Commit)
				if nameLower == "git.exe" || nameLower == "git" {
					cmd, _ := p.Cmdline()
					cmdLower := strings.ToLower(cmd)
					if strings.Contains(cmdLower, "commit") || strings.Contains(cmdLower, "push") || strings.Contains(cmdLower, "pull") {
						isGit = true
					}
				}

				// Detect Tech Stack
				if nameLower == "node.exe" || nameLower == "node" {
					activeTech = "NEXT.JS"
					isCoding = true
				} else if nameLower == "go.exe" || nameLower == "go" {
					activeTech = "GOLANG"
					isCoding = true
				} else if nameLower == "python.exe" || nameLower == "python" {
					activeTech = "PYTHON"
					isCoding = true
				} else if strings.Contains(nameLower, "code.exe") {
					if activeTech == "NONE" {
						activeTech = "VSCODE"
					}
					isCoding = true
				}
			}
		}

		// Xác định State ưu tiên
		newState := "IDLE"
		if temp > 75 || ramUsage > 90 || cpuUsage > 90 {
			newState = "STRESS"
		} else if isGit {
			newState = "THINKING" // Đang push/pull code
		} else if isCoding {
			newState = "CODING"
		}

		// Lời chào theo giờ
		hour := time.Now().Hour()
		greeting := "HELLO, KAI"
		if hour >= 5 && hour < 12 {
			greeting = "GOOD MORNING, KAI"
		} else if hour >= 12 && hour < 18 {
			greeting = "GOOD AFTERNOON, KAI"
		} else if hour >= 18 && hour < 23 {
			greeting = "GOOD EVENING, KAI"
		} else {
			greeting = "LATE NIGHT, KAI"
		}

		// Định dạng đồng hồ cơ: HH:MM:SS | DD/MM/YYYY
		datetime := time.Now().Format("15:04:05 | 02/01/2006")

		// Đã bỏ Weather, rút gọn format: STATE|CPU|RAM|TEMP|GREETING|TECH|DATETIME
		msg := fmt.Sprintf("STATE:%s|%d|%d|%.1f|%s|%s|%s\n", newState, int(cpuUsage), int(ramUsage), temp, greeting, activeTech, datetime)
		port.Write([]byte(msg))

		fmt.Printf("\r🌡️ %.1fC | 🧠 %d%% | 💻 %s | 🐙 Git: %v | 🕒 %s   ", temp, int(ramUsage), activeTech, isGit, datetime)

		// Rút xuống 1 giây để đồng hồ nhảy giây chân thực
		time.Sleep(1 * time.Second)
	}
}
