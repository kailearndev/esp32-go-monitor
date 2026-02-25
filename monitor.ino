#include <lvgl.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

/* ================== CONFIG & BUFFERS ================== */
static const uint16_t screenWidth = 320;
static const uint16_t screenHeight = 240;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * 40];

#define MIC_PIN 4
#define MIC_THRESHOLD 2500

bool isPanicking = false;
unsigned long panicEndTime = 0;

/* ================== UI OBJECTS ================== */
lv_obj_t *screen_eye;
lv_obj_t *eye_l, *eye_r;
lv_obj_t *status_label, *info_label, *top_greeting;
lv_obj_t *tech_badge, *datetime_label; // Đã xóa blush và weather

String current_state = "IDLE";
int current_cpu = 0;
int current_ram = 0;
String last_temp_str = "--";

unsigned long lastLookTime = 0;
unsigned long lastBlinkTime = 0;
unsigned long lastInfoSwitch = 0;
int info_display_state = 0; 

/* ================== HANDLERS ================== */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.startWrite(); tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true); tft.endWrite();
    lv_disp_flush_ready(disp);
}

void anim_x_cb(void *var, int32_t v) { lv_obj_set_style_translate_x((lv_obj_t *)var, v, 0); }
void anim_y_cb(void *var, int32_t v) { lv_obj_set_style_translate_y((lv_obj_t *)var, v, 0); }
void anim_w_cb(void *var, int32_t v) { lv_obj_set_width((lv_obj_t *)var, v); }
void anim_h_cb(void *var, int32_t v) { lv_obj_set_height((lv_obj_t *)var, v); }

void smooth_look(int tx, int ty, int time_ms) {
    lv_anim_t a; lv_anim_init(&a); lv_anim_set_time(&a, time_ms);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    
    lv_anim_set_exec_cb(&a, anim_x_cb);
    lv_anim_set_var(&a, eye_l); lv_anim_set_values(&a, lv_obj_get_style_translate_x(eye_l, 0), tx); lv_anim_start(&a);
    lv_anim_set_var(&a, eye_r); lv_anim_set_values(&a, lv_obj_get_style_translate_x(eye_r, 0), tx); lv_anim_start(&a);
    
    lv_anim_set_exec_cb(&a, anim_y_cb);
    lv_anim_set_var(&a, eye_l); lv_anim_set_values(&a, lv_obj_get_style_translate_y(eye_l, 0), ty); lv_anim_start(&a);
    lv_anim_set_var(&a, eye_r); lv_anim_set_values(&a, lv_obj_get_style_translate_y(eye_r, 0), ty); lv_anim_start(&a);
}

void smooth_size(int w, int h, int radius, int time_ms) {
    lv_anim_t a; lv_anim_init(&a); lv_anim_set_time(&a, time_ms);
    lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
    
    lv_anim_set_exec_cb(&a, anim_w_cb);
    lv_anim_set_var(&a, eye_l); lv_anim_set_values(&a, lv_obj_get_width(eye_l), w); lv_anim_start(&a);
    lv_anim_set_var(&a, eye_r); lv_anim_set_values(&a, lv_obj_get_width(eye_r), w); lv_anim_start(&a);
    
    lv_anim_set_exec_cb(&a, anim_h_cb);
    lv_anim_set_var(&a, eye_l); lv_anim_set_values(&a, lv_obj_get_height(eye_l), h); lv_anim_start(&a);
    lv_anim_set_var(&a, eye_r); lv_anim_set_values(&a, lv_obj_get_height(eye_r), h); lv_anim_start(&a);
    
    lv_obj_set_style_radius(eye_l, radius, 0); lv_obj_set_style_radius(eye_r, radius, 0);
}

void blink_eye() {
    lv_anim_t a; lv_anim_init(&a); lv_anim_set_time(&a, 100);
    lv_anim_set_playback_time(&a, 100); lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&a, anim_h_cb);
    lv_anim_set_var(&a, eye_l); lv_anim_set_values(&a, lv_obj_get_height(eye_l), 4); lv_anim_start(&a);
    lv_anim_set_var(&a, eye_r); lv_anim_set_values(&a, lv_obj_get_height(eye_r), 4); lv_anim_start(&a);
}

void squint_eye() {
    lv_anim_t a; lv_anim_init(&a); lv_anim_set_time(&a, 250); 
    lv_anim_set_playback_time(&a, 200); lv_anim_set_playback_delay(&a, 800); 
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out); lv_anim_set_exec_cb(&a, anim_h_cb);
    lv_anim_set_var(&a, eye_l); lv_anim_set_values(&a, lv_obj_get_height(eye_l), 18); lv_anim_start(&a);
    lv_anim_set_var(&a, eye_r); lv_anim_set_values(&a, lv_obj_get_height(eye_r), 18); lv_anim_start(&a);
}

void apply_state_ui(String tech) {
    lv_color_t color = lv_color_hex(0x00E5FF); 
    String txt = "SYSTEM READY";
    lv_color_t tech_color = lv_color_hex(0x555555);

    if (isPanicking) {
        color = lv_color_hex(0xFFD300); txt = "AHHH! LOUD NOISE!"; 
        smooth_size(20, 70, 10, 150);
    } else if (current_state == "THINKING") {
        color = lv_color_hex(0xFF8C00); // Cam rực
        txt = "SYNCING REPO..."; 
        smooth_size(40, 15, 8, 300); // Nheo mắt nhỏ xíu để suy nghĩ
        smooth_look(15, -20, 300);   // Liếc xéo lên trời
    } else if (current_state == "CODING") {
        color = lv_color_hex(0xB026FF); txt = "WORKSPACE ACTIVE"; 
        smooth_size(50, 35, 10, 300);
        if (tech == "NEXT.JS") tech_color = lv_color_hex(0x61DAFB);
        else if (tech == "GOLANG") tech_color = lv_color_hex(0x00ADD8);
        else if (tech == "PYTHON") tech_color = lv_color_hex(0xFFD43B);
        else tech_color = lv_color_hex(0x007ACC);
    } else if (current_state == "STRESS") {
        color = lv_color_hex(0xFF003C); txt = "SYSTEM OVERLOAD!"; 
        smooth_size(45, 45, 5, 300);
    } else if (current_state == "NIGHT") {
        color = lv_color_hex(0x1A1A2E); txt = "SLEEPING..."; 
        smooth_size(50, 8, 4, 500);
    } else {
        smooth_size(50, 60, 20, 300);
    }

    lv_obj_set_style_bg_color(eye_l, color, 0); lv_obj_set_style_bg_color(eye_r, color, 0);
    lv_label_set_text(status_label, txt.c_str()); lv_obj_set_style_text_color(status_label, color, 0);
    
    if(tech == "NONE" || current_state == "THINKING") {
        lv_label_set_text(tech_badge, ""); 
    } else {
        String badge_str = "[ </> " + tech + " ]";
        lv_label_set_text(tech_badge, badge_str.c_str());
        lv_obj_set_style_text_color(tech_badge, tech_color, 0);
    }
}

void update_sequential_info() {
    if (millis() - lastInfoSwitch > 2500) { 
        info_display_state = (info_display_state + 1) % 3;
        String display_txt = "";
        
        if (info_display_state == 0) display_txt = LV_SYMBOL_SETTINGS " CPU: " + String(current_cpu) + "%";
        else if (info_display_state == 1) display_txt = LV_SYMBOL_SD_CARD " RAM: " + String(current_ram) + "%";
        else display_txt = LV_SYMBOL_CHARGE " TEMP: " + last_temp_str + (last_temp_str == "--" ? "" : "C");

        lv_label_set_text(info_label, display_txt.c_str());
        lv_color_t theme_color = lv_obj_get_style_bg_color(eye_l, 0);
        lv_obj_set_style_text_color(info_label, theme_color, 0);
        lastInfoSwitch = millis();
    }
}

void process_sensors_and_animations() {
    int micValue = analogRead(MIC_PIN);
    if (micValue > MIC_THRESHOLD && !isPanicking) {
        isPanicking = true; panicEndTime = millis() + 2000; apply_state_ui("NONE");
    }
    if (isPanicking && millis() > panicEndTime) {
        isPanicking = false; smooth_look(0, 0, 200); apply_state_ui("NONE");
    }

    if (isPanicking) {
        static unsigned long lastJitter = 0;
        if (millis() - lastJitter > 60) { smooth_look(random(-8, 9), random(-8, 9), 50); lastJitter = millis(); }
    } else if (current_state == "THINKING") {
        // Đóng băng chuyển động liếc ngẫu nhiên, để robot "giữ nguyên" biểu cảm suy nghĩ
    } else {
        if (current_state == "IDLE" || current_state == "CODING") {
            if (millis() - lastBlinkTime > 3500) {
                if (random(0, 10) < 3) squint_eye(); else blink_eye();
                lastBlinkTime = millis();
            }
        }
        if (millis() - lastLookTime > 3000) {
            if (random(0, 2) == 0) smooth_look(random(-30, 31), random(-10, 11), 400);
            else smooth_look(0, 0, 400);
            lastLookTime = millis();
        }
    }
    update_sequential_info();
}

void parseSerial(String data) {
    data.trim();
    if (!data.startsWith("STATE:")) return;

    // Format rút gọn mới: STATE|CPU|RAM|TEMP|GREETING|TECH|DATETIME
    String payload = data.substring(6);
    int p[6]; 
    p[0] = payload.indexOf('|');
    for(int i=1; i<6; i++) { p[i] = payload.indexOf('|', p[i-1] + 1); }

    if (p[0] > 0 && p[5] > p[4]) {
        current_state = payload.substring(0, p[0]);
        current_cpu = payload.substring(p[0] + 1, p[1]).toInt();
        current_ram = payload.substring(p[1] + 1, p[2]).toInt();
        last_temp_str = payload.substring(p[2] + 1, p[3]);
        
        String greeting = payload.substring(p[3] + 1, p[4]);
        String tech = payload.substring(p[4] + 1, p[5]);
        String datetime = payload.substring(p[5] + 1);

        lv_label_set_text(top_greeting, greeting.c_str());
        
        // Hiện giờ đồng hồ cơ
        lv_label_set_text(datetime_label, datetime.c_str());
        
        if (!isPanicking) apply_state_ui(tech);
    }
}

void create_ui() {
    screen_eye = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_eye, lv_color_hex(0x050505), 0);

    // DateTime Clock - Góc phải trên (Font Montserrat để to rõ)
    datetime_label = lv_label_create(screen_eye);
    lv_label_set_text(datetime_label, "--:--:-- | --/--");
    lv_obj_set_style_text_color(datetime_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(datetime_label, &lv_font_montserrat_12, 0);
    lv_obj_align(datetime_label, LV_ALIGN_TOP_RIGHT, -5, 5);

    // Greeting - Góc trái trên (Font bé unscii_8 cho tinh tế)
    top_greeting = lv_label_create(screen_eye);
    lv_label_set_text(top_greeting, "BOOTING...");
    lv_obj_set_style_text_color(top_greeting, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_text_font(top_greeting, &lv_font_unscii_8, 0);
    lv_obj_align(top_greeting, LV_ALIGN_TOP_LEFT, 5, 5);

    lv_obj_t *sep = lv_obj_create(screen_eye);
    lv_obj_set_size(sep, 300, 1);
    lv_obj_align(sep, LV_ALIGN_TOP_MID, 0, 25);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(sep, 0, 0);

    // TẠO MẮT (Đã bỏ má hồng)
    auto setup_eye_obj = [&](lv_obj_t *&e, int x) {
        e = lv_obj_create(screen_eye);
        lv_obj_remove_style_all(e);
        lv_obj_set_style_bg_opa(e, LV_OPA_COVER, 0);
        lv_obj_align(e, LV_ALIGN_CENTER, x, -15);
    };
    setup_eye_obj(eye_l, -45);
    setup_eye_obj(eye_r, 45);

    tech_badge = lv_label_create(screen_eye);
    lv_obj_set_style_text_font(tech_badge, &lv_font_unscii_8, 0);
    lv_obj_align(tech_badge, LV_ALIGN_CENTER, 0, 35);

    status_label = lv_label_create(screen_eye);
    lv_obj_set_style_text_font(status_label, &lv_font_unscii_8, 0);
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_MID, 0, -35);

    info_label = lv_label_create(screen_eye);
    lv_obj_set_style_text_font(info_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(info_label, lv_color_hex(0x505050), 0);
    lv_obj_align(info_label, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_label_set_text(info_label, "[ WAITING DATA ]");
}

void setup() {
    Serial.begin(115200);
    pinMode(MIC_PIN, INPUT);

    tft.begin(); tft.setRotation(1);
    lv_init(); lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 40);
    static lv_disp_drv_t d; lv_disp_drv_init(&d);
    d.hor_res = screenWidth; d.ver_res = screenHeight;
    d.flush_cb = my_disp_flush; d.draw_buf = &draw_buf;
    lv_disp_drv_register(&d);

    create_ui();
    lv_scr_load(screen_eye);
    apply_state_ui("NONE");
}

void loop() {
    if (Serial.available()) {
        parseSerial(Serial.readStringUntil('\n'));
    }
    process_sensors_and_animations();
    lv_timer_handler(); delay(5);
}