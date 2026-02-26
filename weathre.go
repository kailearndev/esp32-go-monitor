package main

import (
	"encoding/json"
	"fmt"
	"io"
	"log"
	"math/rand"
	"net/http"
	"time"

	"go.bug.st/serial"
)

// Khai báo Struct để hứng data từ JSON Open-Meteo API
type WeatherResponse struct {
	Current struct {
		Temperature float64 `json:"temperature_2m"`
		Humidity    int     `json:"relative_humidity_2m"`
		WeatherCode int     `json:"weather_code"`
	} `json:"current"`
}

// Hàm map mã thời tiết sang text (Đã chỉnh lại để ESP32 nhận diện đúng Icon Nắng/Mưa/Mây)
func weatherCodeToString(code int) string {
	switch code {
	case 0:
		return "Clear"
	case 1, 2:
		return "Mostly Clear"
	case 3:
		return "Overcast"
	case 45, 48:
		return "Foggy"
	case 51, 53, 55:
		return "Light Rain" // Có chữ "Rain" để ESP32 hiện icon mưa
	case 61, 63, 65:
		return "Rain"
	case 71, 73, 75:
		return "Snow"
	case 77:
		return "Snow Grains"
	case 80, 81, 82:
		return "Rain Showers"
	case 85, 86:
		return "Snow Showers"
	case 95, 96, 99:
		return "Rain Storm" // Có chữ "Rain" để ESP32 hiện icon mưa
	default:
		return "Unknown"
	}
}

func main() {
	// 1. Cấu hình Serial
	mode := &serial.Mode{
		BaudRate: 115200,
	}

	// TODO: Nhớ kiểm tra lại cổng COM của máy cty nha
	portName := "COM3"

	port, err := serial.Open(portName, mode)
	if err != nil {
		log.Fatalf("Khóc thét! Không mở được cổng Serial %s: %v", portName, err)
	}
	defer port.Close()

	fmt.Println("🚀 Gopher đã kết nối với ESP32! Bắt đầu fetch API Thời tiết...")

	// 2. Goroutine lấy thời tiết thật (Cập nhật 10 phút / lần)
	go func() {
		// Tọa độ công ty ở Sài Gòn
		lat := "10.7817187"
		lon := "106.6866875"
		apiURL := fmt.Sprintf("https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s&current=temperature_2m,relative_humidity_2m,weather_code&timezone=Asia/Ho_Chi_Minh",
			lat, lon)

		for {
			resp, err := http.Get(apiURL)
			if err != nil {
				fmt.Println("Lỗi gọi API Thời tiết:", err)
				time.Sleep(30 * time.Second)
				continue
			}

			body, _ := io.ReadAll(resp.Body)
			resp.Body.Close()

			if resp.StatusCode != 200 {
				fmt.Printf("❌ API trả về lỗi (Status %d): %s\n", resp.StatusCode, string(body))
				time.Sleep(30 * time.Second)
				continue
			}

			var weatherData WeatherResponse
			if err := json.Unmarshal(body, &weatherData); err == nil {
				
				t := fmt.Sprintf("%.0f", weatherData.Current.Temperature)
				c := weatherCodeToString(weatherData.Current.WeatherCode)
				h := fmt.Sprintf("%d%%", weatherData.Current.Humidity)
				
				// Đổi tên Location cho ngầu
				loc := "DYM HCMC"

				// Format: W:28|Clear|70%|DYM HCMC\n
				weatherStr := fmt.Sprintf("W:%s|%s|%s|%s\n", t, c, h, loc)
				port.Write([]byte(weatherStr))
				fmt.Print("☁️ Đã cập nhật Weather: ", weatherStr)
			} else {
				fmt.Println("Lỗi parse JSON:", err)
			}

			time.Sleep(10 * time.Minute)
		}
	}()

	// 3. Goroutine Coding Meme (Tạm chạy random để UI nhảy số)
	go func() {
		techs := []string{"React", "Next.js", "Golang", "Node.js", "Bug", "Idle"}
		for {
			tech := techs[rand.Intn(len(techs))]
			codingStr := fmt.Sprintf("C:%s\n", tech)
			port.Write([]byte(codingStr))

			if tech == "Bug" {
				time.Sleep(5 * time.Second)
			} else {
				time.Sleep(3 * time.Second)
			}
		}
	}()

	select {}
}