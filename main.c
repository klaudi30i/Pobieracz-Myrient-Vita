#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <psp2/kernel/process.h>
#include <psp2/ctrl.h>
#include <psp2/io/fcntl.h>
#include <psp2/net/net.h>
#include <psp2/sysmodule.h>
#include <vita2d.h>
#include <curl/curl.h>

#define BASE_URL "https://myrient.erista.me/files/"
#define SAVE_DIR "ux0:downloads/"

/* Kolory */
#define COL_BG    RGBA8(30, 30, 35, 255)
#define COL_TEXT  RGBA8(255, 255, 255, 255)
#define COL_DIR   RGBA8(255, 220, 0, 255)
#define COL_SEL   RGBA8(0, 150, 255, 255)

typedef struct {
    char name[128];
    char href[256];
    int is_folder;
} Entry;

Entry *entries = NULL;
int entryCount = 0;
int selected = 0;
int scroll = 0;
char currentUrl[1024];
char status[256] = "Wczytywanie...";
int downloading = 0;
float progress = 0.0f;

struct MemoryStruct { char *memory; size_t size; };

static size_t WriteMem(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(!ptr) return 0;
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

void parse_html(char *html) {
    if (entries) free(entries);
    entries = malloc(sizeof(Entry) * 2048);
    entryCount = 0;
    selected = 0;
    scroll = 0;

    char *cursor = html;
    if (strcmp(currentUrl, BASE_URL) != 0) {
        strcpy(entries[0].name, "[ .. ] COFNIJ");
        strcpy(entries[0].href, "../");
        entries[0].is_folder = 1;
        entryCount++;
    }

    while ((cursor = strstr(cursor, "<a href=\"")) != NULL) {
        cursor += 9;
        char *endQuote = strchr(cursor, '"');
        if (!endQuote) break;
        int len = endQuote - cursor;
        if (len > 255) len = 255;
        
        char href[256];
        strncpy(href, cursor, len);
        href[len] = '\0';
        cursor = endQuote;

        if (href[0] == '?') continue;
        if (strcmp(href, "../") == 0) continue; 

        int is_dir = (href[len-1] == '/');
        int is_zip = (strstr(href, ".zip") != NULL);

        if (is_dir || is_zip) {
            strncpy(entries[entryCount].href, href, 255);
            char display_name[128];
            strncpy(display_name, href, 127);
            if(is_dir && len > 0) display_name[len-1] = '\0';
            
            snprintf(entries[entryCount].name, 128, "%s%s", is_dir ? "[DIR] " : "", display_name);
            entries[entryCount].is_folder = is_dir;
            entryCount++;
            if (entryCount >= 2048) break;
        }
    }
    snprintf(status, 256, "Plikow: %d", entryCount);
}

void fetch_dir() {
    CURL *curl;
    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;
    
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, currentUrl);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMem);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "VitaBrowser/1.0");
        curl_easy_perform(curl);
        parse_html(chunk.memory);
        curl_easy_cleanup(curl);
        free(chunk.memory);
    }
}

size_t write_file(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    return fwrite(ptr, size, nmemb, stream);
}

int progress_cb(void *p, double dlt, double dln, double ult, double uln) {
    if (dlt > 0) progress = (float)(dln / dlt);
    
    vita2d_start_drawing();
    vita2d_clear_screen();
    vita2d_draw_rectangle(100, 250, 760, 40, RGBA8(50,50,50,255));
    vita2d_draw_rectangle(100, 250, (int)(760 * progress), 40, RGBA8(0,255,0,255));
    vita2d_end_drawing();
    vita2d_swap_buffers();
    return 0;
}

void download_file(char *filename, char *full_url) {
    CURL *curl;
    FILE *fp;
    char path[256];
    snprintf(path, 256, "%s%s", SAVE_DIR, filename);

    curl = curl_easy_init();
    if (curl) {
        fp = fopen(path, "wb");
        if(fp) {
            downloading = 1;
            curl_easy_setopt(curl, CURLOPT_URL, full_url);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, progress_cb);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
            curl_easy_perform(curl);
            fclose(fp);
            downloading = 0;
        }
        curl_easy_cleanup(curl);
    }
}

void handle_enter() {
    if (entryCount == 0) return;
    Entry *e = &entries[selected];
    if (e->is_folder) {
        if (strcmp(e->href, "../") == 0) {
            char *last = strrchr(currentUrl, '/');
            if (last) *last = '\0';
            last = strrchr(currentUrl, '/');
            if (last) *(last+1) = '\0';
            if (strlen(currentUrl) < strlen(BASE_URL)) strcpy(currentUrl, BASE_URL);
        } else {
            strcat(currentUrl, e->href);
        }
        fetch_dir();
    } else {
        char full_url[1024];
        snprintf(full_url, 1024, "%s%s", currentUrl, e->href);
        download_file(e->href, full_url);
        snprintf(status, 256, "Pobrano: %s", e->name);
    }
}

int main() {
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    SceNetInitParam initparam;
    initparam.memory = malloc(1024*1024); initparam.size = 1024*1024; initparam.flags=0;
    sceNetInit(&initparam);
    sceIoMkdir(SAVE_DIR, 0777);
    
    vita2d_init();
    vita2d_set_clear_color(COL_BG);
    vita2d_pgf *pgf = vita2d_load_default_pgf();
    
    strcpy(currentUrl, BASE_URL);
    fetch_dir();

    SceCtrlData pad;
    while (1) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        
        if (pad.buttons & SCE_CTRL_DOWN) {
            if (selected < entryCount - 1) selected++;
            if (selected > scroll + 18) scroll++;
            sceKernelDelayThread(150000);
        }
        if (pad.buttons & SCE_CTRL_UP) {
            if (selected > 0) selected--;
            if (selected < scroll) scroll--;
            sceKernelDelayThread(150000);
        }
        if (pad.buttons & SCE_CTRL_CROSS) {
            handle_enter();
            sceKernelDelayThread(300000);
        }

        vita2d_start_drawing();
        vita2d_clear_screen();
        
        vita2d_pgf_draw_text(pgf, 10, 25, COL_TEXT, 1.0f, currentUrl);
        vita2d_draw_line(0, 35, 960, 35, COL_TEXT);

        int y = 55;
        for(int i=scroll; i<entryCount && i<scroll+20; i++) {
            unsigned int c = (i == selected) ? COL_SEL : (entries[i].is_folder ? COL_DIR : COL_TEXT);
            vita2d_pgf_draw_text(pgf, 20, y, c, 1.0f, entries[i].name);
            y += 22;
        }
        
        vita2d_pgf_draw_text(pgf, 10, 530, RGBA8(0,255,0,255), 1.0f, status);
        
        vita2d_end_drawing();
        vita2d_swap_buffers();
    }
    return 0;
}
