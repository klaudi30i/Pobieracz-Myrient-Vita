#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/sysmodule.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/power.h>
#include <vita2d.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// --- KONFIGURACJA ---
#define NET_MEM_SIZE (4 * 1024 * 1024)
static char net_memory[NET_MEM_SIZE] __attribute__((aligned(256)));

#define MAX_ENTRIES 8192
#define NET_BUF_SIZE 262144
#define VISIBLE_ROWS 13

const char *ROOT_URL = "https://myrient.erista.me/files/";
const char *LIST_FILE = "ux0:downloads/myrient/list.html";
const char *USER_AGENT = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";

// --- KOLORY ---
#define RGBA8(r,g,b,a) ((((a)&0xFF)<<24) | (((b)&0xFF)<<16) | (((g)&0xFF)<<8) | (((r)&0xFF)<<0))
#define COL_BG          RGBA8(30, 30, 35, 255)
#define COL_HEADER      RGBA8(45, 45, 50, 255)
#define COL_ROW_ODD     RGBA8(35, 35, 40, 255)
#define COL_ROW_EVEN    RGBA8(30, 30, 35, 255)
#define COL_SELECT      RGBA8(0, 120, 215, 255)
#define COL_TEXT        RGBA8(220, 220, 220, 255)
#define COL_TEXT_SEL    RGBA8(255, 255, 255, 255)
#define COL_FOLDER      RGBA8(240, 200, 80, 255)
#define COL_FILE        RGBA8(150, 160, 170, 255)
#define COL_SCROLL_BG   RGBA8(20, 20, 20, 255)
#define COL_SCROLL_BAR  RGBA8(100, 100, 100, 255)
#define COL_ACCENT      RGBA8(0, 200, 100, 255)
#define COLOR_RED       RGBA8(255, 80, 80, 255)
#define COL_KB_BG       RGBA8(40, 40, 45, 255)
#define COL_KEY         RGBA8(60, 60, 70, 255)

// Kolory Przycisków PlayStation
#define COL_BTN_CROSS    RGBA8(64, 128, 255, 255)  // Niebieski
#define COL_BTN_CIRCLE   RGBA8(255, 64, 64, 255)   // Czerwony
#define COL_BTN_TRIANGLE RGBA8(64, 255, 128, 255)  // Zielony
#define COL_BTN_SQUARE   RGBA8(255, 128, 255, 255) // Różowy

// --- ZMIENNE GLOBALNE ---
volatile double dl_now = 0;
volatile double dl_total = 0;
volatile int is_working = 0;
volatile int cancel_request = 0;
char global_url[2048];
char global_path[512];
char global_referer[2048];
SceUID file_handle = -1;
char last_error[256] = "Gotowy";

typedef struct { 
    char name[256]; 
    char url_part[512]; 
    int is_folder; 
} Entry;

Entry entries[MAX_ENTRIES];
int entries_count = 0;
int filtered_indices[MAX_ENTRIES];
int filtered_count = 0;
char search_query[128] = "";

vita2d_pgf *pgf;
char current_url[2048];

// --- RYSOWANIE SYMBOLI ---
// Funkcja rysuje symbol w punkcie x, y (rozmiar ok. 20x20)
// 0: Cross (X), 1: Circle (O), 2: Triangle (^), 3: Square ([])
void draw_symbol(int x, int y, int type) {
    int s = 20; // Size
    int th = 3; // Thickness
    
    switch(type) {
        case 0: // CROSS (X)
            vita2d_draw_line(x, y, x+s, y+s, COL_BTN_CROSS);
            vita2d_draw_line(x+1, y, x+s+1, y+s, COL_BTN_CROSS); // Pogrubienie
            vita2d_draw_line(x+s, y, x, y+s, COL_BTN_CROSS);
            vita2d_draw_line(x+s+1, y, x+1, y+s, COL_BTN_CROSS); // Pogrubienie
            break;
        case 1: // CIRCLE (O) - Rysujemy duże koło i mniejsze w środku w kolorze tła
            vita2d_draw_fill_circle(x + s/2, y + s/2, s/2, COL_BTN_CIRCLE);
            vita2d_draw_fill_circle(x + s/2, y + s/2, s/2 - th, COL_HEADER); // Wycinka środka
            break;
        case 2: // TRIANGLE
            vita2d_draw_line(x + s/2, y, x, y+s, COL_BTN_TRIANGLE);
            vita2d_draw_line(x + s/2, y, x+s, y+s, COL_BTN_TRIANGLE);
            vita2d_draw_line(x, y+s, x+s, y+s, COL_BTN_TRIANGLE);
            // Pogrubienie podstawy
            vita2d_draw_line(x, y+s-1, x+s, y+s-1, COL_BTN_TRIANGLE);
            break;
        case 3: // SQUARE
            vita2d_draw_rectangle(x, y, s, th, COL_BTN_SQUARE);          // Góra
            vita2d_draw_rectangle(x, y+s-th, s, th, COL_BTN_SQUARE);     // Dół
            vita2d_draw_rectangle(x, y, th, s, COL_BTN_SQUARE);          // Lewo
            vita2d_draw_rectangle(x+s-th, y, th, s, COL_BTN_SQUARE);     // Prawo
            break;
    }
}

// --- KLAWIATURA ---
const char *kb_layout[] = { "1234567890", "QWERTYUIOP", "ASDFGHJKL-", "ZXCVBNM ._" };
int kb_row = 1; int kb_col = 0; int touch_cooldown = 0; 

char check_keyboard_touch(int tx, int ty) {
    int start_y = 350; int key_w = 80; int key_h = 40; int gap = 10;
    for(int r=0; r<4; r++) {
        int len = strlen(kb_layout[r]);
        int row_width = len * key_w + (len-1)*gap;
        int start_x = (960 - row_width) / 2;
        if (ty >= start_y + r*(key_h+gap) && ty <= start_y + r*(key_h+gap) + key_h) {
            for(int c=0; c<len; c++) {
                int x = start_x + c * (key_w + gap);
                if (tx >= x && tx <= x + key_w) { kb_row = r; kb_col = c; return kb_layout[r][c]; }
            }
        }
    }
    return 0;
}

void draw_custom_keyboard() {
    vita2d_draw_rectangle(0, 280, 960, 264, COL_KB_BG);
    vita2d_draw_rectangle(0, 280, 960, 2, COL_ACCENT);
    vita2d_pgf_draw_text(pgf, 30, 310, COL_ACCENT, 1.2f, "Szukaj:");
    char buf[140]; sprintf(buf, "%s_", search_query); 
    vita2d_pgf_draw_text(pgf, 130, 310, COL_TEXT_SEL, 1.2f, buf);
    
    // Instrukcje z symbolami
    int inst_y = 315;
    draw_symbol(600, inst_y-15, 3); // Square
    vita2d_pgf_draw_text(pgf, 630, inst_y, COL_TEXT, 0.9f, "Spacja");
    
    draw_symbol(720, inst_y-15, 2); // Triangle
    vita2d_pgf_draw_text(pgf, 750, inst_y, COL_TEXT, 0.9f, "Backspace");
    
    vita2d_pgf_draw_text(pgf, 600, inst_y+20, COL_TEXT, 0.9f, "START: Szukaj/Zamknij");

    int start_y = 350; int key_w = 80; int key_h = 40; int gap = 10;
    for(int r=0; r<4; r++) {
        int len = strlen(kb_layout[r]);
        int row_width = len * key_w + (len-1)*gap;
        int start_x = (960 - row_width) / 2;
        for(int c=0; c<len; c++) {
            int x = start_x + c * (key_w + gap);
            int y = start_y + r * (key_h + gap);
            unsigned int color = (r == kb_row && c == kb_col) ? COL_SELECT : COL_KEY;
            vita2d_draw_rectangle(x, y, key_w, key_h, color);
            char key_char[2] = {kb_layout[r][c], '\0'};
            vita2d_pgf_draw_text(pgf, x + 35, y + 28, COL_TEXT_SEL, 1.2f, key_char);
        }
    }
}

// --- RESZTA FUNKCJI (BEZ ZMIAN) ---
int str_contains_nocase(const char *haystack, const char *needle) {
    if (!needle || !needle[0]) return 1;
    const char *h, *n;
    for (; *haystack; haystack++) {
        h = haystack; n = needle;
        while (*h && *n && (tolower((unsigned char)*h) == tolower((unsigned char)*n))) { h++; n++; }
        if (!*n) return 1;
    }
    return 0;
}
void update_search() {
    filtered_count = 0;
    for (int i = 0; i < entries_count; i++) {
        if (str_contains_nocase(entries[i].name, search_query)) filtered_indices[filtered_count++] = i;
    }
}
size_t header_cb(char *buffer, size_t size, size_t nitems, void *userdata) {
    char *ptr = strstr(buffer, "Content-Length");
    if (ptr) { unsigned long long len = 0; sscanf(ptr, "Content-Length: %llu", &len); if (len > 0) dl_total = len; }
    return nitems;
}
size_t write_cb(void *ptr, size_t size, size_t nmemb, void *stream) {
    if (cancel_request) return 0;
    size_t realsize = size * nmemb;
    if (file_handle >= 0) sceIoWrite(file_handle, ptr, realsize);
    dl_now += realsize; return realsize;
}
int worker_thread(unsigned int args, void *arg) {
    CURL *curl = curl_easy_init();
    if (!curl) { sprintf(last_error, "Blad: Curl init"); is_working = 0; return 0; }
    file_handle = sceIoOpen(global_path, SCE_O_WRONLY | SCE_O_TRUNC | SCE_O_CREAT, 0777);
    if (file_handle < 0) { sprintf(last_error, "Blad: IO Error"); curl_easy_cleanup(curl); is_working = 0; return 0; }
    dl_now = 0; dl_total = 0;
    curl_easy_setopt(curl, CURLOPT_URL, global_url); curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT); curl_easy_setopt(curl, CURLOPT_REFERER, global_referer);
    curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1L); curl_easy_setopt(curl, CURLOPT_COOKIEFILE, ""); 
    curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2); curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, (long)NET_BUF_SIZE); curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L); curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb); curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_cb);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK && !cancel_request && dl_total > 0 && dl_now < dl_total) { sceKernelDelayThread(200000); curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, (curl_off_t)dl_now); res = curl_easy_perform(curl); }
    sceIoClose(file_handle); file_handle = -1; curl_easy_cleanup(curl);
    if (res != CURLE_OK) sprintf(last_error, "Blad sieci: %d", res); else sprintf(last_error, "OK");
    is_working = 0; return sceKernelExitDeleteThread(0);
}
void start_task(const char *url, const char *path, const char *referer) {
    if (is_working) return;
    strcpy(global_url, url); strcpy(global_path, path);
    if (referer) strcpy(global_referer, referer); else strcpy(global_referer, "");
    cancel_request = 0; is_working = 1; sprintf(last_error, "Pobieranie...");
    SceUID thd = sceKernelCreateThread("MyrientWorker", worker_thread, 0x10000100, 0x100000, 0, 0, NULL);
    sceKernelStartThread(thd, 0, NULL);
}
void parse_list() {
    FILE *fp = fopen(LIST_FILE, "r");
    if (!fp) { sprintf(last_error, "Brak pliku listy"); entries_count = 0; update_search(); return; }
    fseek(fp, 0, SEEK_END); long fsize = ftell(fp); rewind(fp);
    if (fsize < 50) { if(strstr(last_error, "OK")) sprintf(last_error, "Pusty plik listy"); fclose(fp); entries_count = 0; update_search(); return; }
    char line[4096]; entries_count = 0; CURL *curl = curl_easy_init();
    while (fgets(line, sizeof(line), fp)) {
        if (entries_count >= MAX_ENTRIES) break;
        char *href_pos = strstr(line, "href=\"");
        if (href_pos) {
            char *start = href_pos + 6; char *end = strstr(start, "\"");
            if (end) {
                *end = '\0'; if (start[0] == '?' || start[0] == '/' || strstr(start, "http") || strcmp(start, "../") == 0 || strcmp(start, "./") == 0) continue;
                strncpy(entries[entries_count].url_part, start, 511); entries[entries_count].url_part[511] = '\0';
                int len = strlen(start); entries[entries_count].is_folder = (start[len-1] == '/'); if (entries[entries_count].is_folder) start[len-1] = '\0';
                int outlen; char *decoded = curl_easy_unescape(curl, start, 0, &outlen);
                strncpy(entries[entries_count].name, decoded ? decoded : start, 255); entries[entries_count].name[255] = '\0';
                if(decoded) curl_free(decoded); entries_count++;
            }
        }
    }
    if(curl) curl_easy_cleanup(curl); fclose(fp);
    if (entries_count > 0) sprintf(last_error, "Lista OK (%d)", entries_count);
    strcpy(search_query, ""); update_search();
}
void go_back() {
    if (strcmp(current_url, ROOT_URL) == 0) return;
    int len = strlen(current_url); if (current_url[len-1] == '/') current_url[len-1] = '\0';
    char *last_slash = strrchr(current_url, '/'); if (last_slash) *(last_slash + 1) = '\0'; else strcpy(current_url, ROOT_URL);
}

// --- MAIN ---
int main(int argc, char *argv[]) {
    scePowerSetArmClockFrequency(444); scePowerSetBusClockFrequency(222); scePowerSetGpuClockFrequency(222);
    sceIoMkdir("ux0:downloads", 0777); sceIoMkdir("ux0:downloads/myrient", 0777);
    vita2d_init(); pgf = vita2d_load_default_pgf();
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    SceNetInitParam netInitParam = {net_memory, NET_MEM_SIZE, 0};
    sceNetInit(&netInitParam); sceNetCtlInit();
    curl_global_init(CURL_GLOBAL_ALL);
    strcpy(current_url, ROOT_URL);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);

    SceCtrlData pad; SceTouchData touch;
    int selected = 0, old_buttons = 0, scroll_offset = 0;
    int state = 1; int scroll_timer = 0;
    #define SCROLL_DELAY_INITIAL 15
    #define SCROLL_SPEED 2

    start_task(current_url, LIST_FILE, ROOT_URL);

    while (1) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        sceTouchPeek(0, &touch, 1);
        if (is_working) sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);

        if (state == 3) { // --- KLAWIATURA ---
            char input_char = 0;
            if (touch.reportNum > 0) {
                if (touch_cooldown == 0) {
                    input_char = check_keyboard_touch(touch.report[0].x / 2, touch.report[0].y / 2);
                    if (input_char) touch_cooldown = 15;
                } else touch_cooldown--;
            } else touch_cooldown = 0;

            if ((pad.buttons & SCE_CTRL_RIGHT) && !(old_buttons & SCE_CTRL_RIGHT)) kb_col = (kb_col + 1) % strlen(kb_layout[kb_row]);
            if ((pad.buttons & SCE_CTRL_LEFT) && !(old_buttons & SCE_CTRL_LEFT)) kb_col = (kb_col - 1 + strlen(kb_layout[kb_row])) % strlen(kb_layout[kb_row]);
            if ((pad.buttons & SCE_CTRL_DOWN) && !(old_buttons & SCE_CTRL_DOWN)) { kb_row = (kb_row + 1) % 4; if (kb_col >= strlen(kb_layout[kb_row])) kb_col = strlen(kb_layout[kb_row])-1; }
            if ((pad.buttons & SCE_CTRL_UP) && !(old_buttons & SCE_CTRL_UP)) { kb_row = (kb_row + 3) % 4; if (kb_col >= strlen(kb_layout[kb_row])) kb_col = strlen(kb_layout[kb_row])-1; }

            if (((pad.buttons & SCE_CTRL_CROSS) && !(old_buttons & SCE_CTRL_CROSS)) || input_char) {
                char c = input_char ? input_char : kb_layout[kb_row][kb_col];
                int len = strlen(search_query); if (len < 63) { search_query[len] = c; search_query[len+1] = '\0'; update_search(); selected = 0; scroll_offset = 0; }
            }
            if ((pad.buttons & SCE_CTRL_SQUARE) && !(old_buttons & SCE_CTRL_SQUARE)) { int len = strlen(search_query); if(len<63) { search_query[len]=' '; search_query[len+1]='\0'; update_search(); } }
            if ((pad.buttons & SCE_CTRL_TRIANGLE) && !(old_buttons & SCE_CTRL_TRIANGLE)) { int len = strlen(search_query); if(len>0) { search_query[len-1]='\0'; update_search(); selected = 0; scroll_offset = 0; } }
            if ((pad.buttons & SCE_CTRL_START) && !(old_buttons & SCE_CTRL_START)) state = 0;
            if ((pad.buttons & SCE_CTRL_CIRCLE) && !(old_buttons & SCE_CTRL_CIRCLE)) state = 0;
        }
        else if (state == 0) { // Browser
            if ((pad.buttons & SCE_CTRL_SQUARE) && !(old_buttons & SCE_CTRL_SQUARE)) { state = 3; kb_row = 1; kb_col = 0; }
            if ((pad.buttons & SCE_CTRL_TRIANGLE) && !(old_buttons & SCE_CTRL_TRIANGLE)) {
                if (strlen(search_query) > 0) { strcpy(search_query, ""); update_search(); }
                else if (strcmp(current_url, ROOT_URL) != 0) { go_back(); start_task(current_url, LIST_FILE, current_url); state = 1; }
            }
            int move = 0;
            if (pad.buttons & SCE_CTRL_DOWN) { if (!(old_buttons & SCE_CTRL_DOWN)) { move = 1; scroll_timer = 0; } else { scroll_timer++; if (scroll_timer > SCROLL_DELAY_INITIAL && (scroll_timer % SCROLL_SPEED == 0)) move = 1; } }
            else if (pad.buttons & SCE_CTRL_UP) { if (!(old_buttons & SCE_CTRL_UP)) { move = -1; scroll_timer = 0; } else { scroll_timer++; if (scroll_timer > SCROLL_DELAY_INITIAL && (scroll_timer % SCROLL_SPEED == 0)) move = -1; } } else { scroll_timer = 0; }
            if (move != 0 && filtered_count > 0) {
                selected += move;
                if (selected >= filtered_count) { selected = 0; scroll_offset = 0; }
                else if (selected < 0) { selected = filtered_count - 1; scroll_offset = (filtered_count > VISIBLE_ROWS) ? filtered_count - VISIBLE_ROWS : 0; }
                if (selected >= scroll_offset + VISIBLE_ROWS) scroll_offset = selected - VISIBLE_ROWS + 1; if (selected < scroll_offset) scroll_offset = selected;
            }
            if (pad.buttons & SCE_CTRL_RIGHT && !(old_buttons & SCE_CTRL_RIGHT)) { selected += 10; if (selected >= filtered_count) selected = filtered_count - 1; if (selected >= scroll_offset + VISIBLE_ROWS) scroll_offset = selected - VISIBLE_ROWS + 1; if (scroll_offset < 0) scroll_offset = 0; }
            if (pad.buttons & SCE_CTRL_LEFT && !(old_buttons & SCE_CTRL_LEFT)) { selected -= 10; if (selected < 0) selected = 0; if (selected < scroll_offset) scroll_offset = selected; }
            if ((pad.buttons & SCE_CTRL_CROSS) && !(old_buttons & SCE_CTRL_CROSS) && filtered_count > 0) {
                int real_idx = filtered_indices[selected];
                if (entries[real_idx].is_folder) {
                    strcat(current_url, entries[real_idx].url_part); int len = strlen(entries[real_idx].url_part);
                    if (entries[real_idx].url_part[len-1] != '/') strcat(current_url, "/");
                    strcpy(search_query, ""); start_task(current_url, LIST_FILE, current_url); state = 1;
                } else {
                    char file_url[2048], save_path[512]; sprintf(file_url, "%s%s", current_url, entries[real_idx].url_part);
                    CURL *c = curl_easy_init(); int ol; char *clean = curl_easy_unescape(c, entries[real_idx].name, 0, &ol);
                    sprintf(save_path, "ux0:downloads/myrient/%s", clean ? clean : entries[real_idx].name);
                    if(clean) curl_free(clean); curl_easy_cleanup(c); start_task(file_url, save_path, current_url); state = 2;
                }
            }
        }
        else if (state == 1) { if (!is_working) { if (strstr(last_error, "OK")) { parse_list(); selected = 0; scroll_offset = 0; } state = 0; } }
        else if (state == 2) { if (!is_working) state = 0; if ((pad.buttons & SCE_CTRL_CIRCLE) && !(old_buttons & SCE_CTRL_CIRCLE)) cancel_request = 1; }

        vita2d_start_drawing(); vita2d_clear_screen(); vita2d_draw_rectangle(0, 0, 960, 544, COL_BG);
        vita2d_draw_rectangle(0, 0, 960, 50, COL_HEADER);
        vita2d_pgf_draw_text(pgf, 20, 35, COL_ACCENT, 1.2f, "MYRIENT");
        char path_display[128]; if (strlen(current_url) > 60) sprintf(path_display, "...%s", current_url + strlen(current_url) - 60); else strcpy(path_display, current_url);
        vita2d_pgf_draw_text(pgf, 150, 35, COL_TEXT, 1.0f, path_display);

        int row_h = 32; int list_y_start = 60; int max_visible = (state == 3) ? 6 : VISIBLE_ROWS;
        if (state == 0 || state == 3 || (state == 1 && filtered_count > 0)) {
            for (int i = 0; i < max_visible; i++) {
                int list_idx = scroll_offset + i; if (list_idx >= filtered_count) break;
                int entry_idx = filtered_indices[list_idx]; int y_pos = list_y_start + (i * row_h);
                unsigned int bg_col = (i % 2 == 0) ? COL_ROW_EVEN : COL_ROW_ODD; unsigned int txt_col = COL_TEXT;
                if (list_idx == selected && state == 0) { bg_col = COL_SELECT; txt_col = COL_TEXT_SEL; }
                vita2d_draw_rectangle(0, y_pos, 940, row_h, bg_col);
                if (entries[entry_idx].is_folder) { vita2d_draw_rectangle(15, y_pos + 6, 22, 20, COL_FOLDER); vita2d_draw_rectangle(15, y_pos + 4, 10, 4, COL_FOLDER); } 
                else { vita2d_draw_rectangle(18, y_pos + 6, 16, 20, COL_FILE); }
                vita2d_pgf_draw_text(pgf, 50, y_pos + 24, txt_col, 1.0f, entries[entry_idx].name);
            }
            if (filtered_count > max_visible) {
                int scroll_h = max_visible * row_h; int bar_h = (max_visible * scroll_h) / filtered_count; if (bar_h < 20) bar_h = 20;
                int bar_y = 60 + (scroll_offset * (scroll_h - bar_h)) / (filtered_count - max_visible);
                vita2d_draw_rectangle(945, 60, 15, scroll_h, COL_SCROLL_BG); vita2d_draw_rectangle(947, bar_y, 11, bar_h, COL_SCROLL_BAR);
            }
        }
        if (state == 3) draw_custom_keyboard();
        else { 
            // --- RYSOWANIE STOPKI (Bez Stringa - Ikony + Tekst) ---
            vita2d_draw_rectangle(0, 504, 960, 40, COL_HEADER);
            int fy = 532;
            char buf[64]; sprintf(buf, "Pliki: %d", filtered_count);
            vita2d_pgf_draw_text(pgf, 20, fy, COL_TEXT, 0.9f, buf);

            // X - Wybierz
            draw_symbol(200, fy-20, 0); // 0 = Cross
            vita2d_pgf_draw_text(pgf, 230, fy, COL_TEXT, 0.9f, "Wybierz");

            // Triangle - Wstecz
            draw_symbol(360, fy-20, 2); // 2 = Triangle
            vita2d_pgf_draw_text(pgf, 390, fy, COL_TEXT, 0.9f, "Wstecz");

            // Square - Szukaj
            draw_symbol(500, fy-20, 3); // 3 = Square
            vita2d_pgf_draw_text(pgf, 530, fy, COL_TEXT, 0.9f, "Szukaj");
        }

        if (state == 1 || state == 2) {
             vita2d_draw_rectangle(0, 0, 960, 544, RGBA8(0,0,0,180)); int win_x = 180, win_y = 172, win_w = 600;
             vita2d_draw_rectangle(win_x, win_y, win_w, 200, COL_HEADER); vita2d_draw_rectangle(win_x, win_y, win_w, 2, COL_ACCENT);
             if(state==1) { vita2d_pgf_draw_text(pgf, win_x+20, win_y+40, COL_TEXT, 1.2f, "Wczytywanie..."); if(strstr(last_error, "Blad")) vita2d_pgf_draw_text(pgf, win_x+20, win_y+100, COLOR_RED, 1.0f, last_error); } 
             else { vita2d_pgf_draw_text(pgf, win_x+20, win_y+40, COL_TEXT, 1.2f, "Pobieranie pliku"); if(dl_total>0) { double pct = dl_now/dl_total; vita2d_draw_rectangle(win_x+20, win_y+80, win_w-40, 30, COL_SCROLL_BG); vita2d_draw_rectangle(win_x+20, win_y+80, (win_w-40)*pct, 30, COL_ACCENT); char pt[128]; sprintf(pt, "%.2f MB / %.2f MB (%.1f%%)", dl_now/1024/1024.0, dl_total/1024/1024.0, pct*100); vita2d_pgf_draw_text(pgf, win_x+20, win_y+140, COL_TEXT, 1.0f, pt); } }
        }
        vita2d_end_drawing(); vita2d_swap_buffers(); old_buttons = pad.buttons;
    }
    return 0;
}