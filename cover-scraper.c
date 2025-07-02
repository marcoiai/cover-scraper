#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

#define CLIENT_ID "8vbed02973d4fiwe1z1zfe45gm394c"
#define CLIENT_SECRET "ettee2371fahx52q8q4dr7g1ujrewu"
#define ROMS_DIR "./roms"
#define COVERS_DIR "./covers"
#define TOKEN_URL "https://id.twitch.tv/oauth2/token"
#define IGDB_API_URL "https://api.igdb.com/v4/games"

struct MemoryStruct {
    char *memory;
    size_t size;
};

void clear_covers_directory() {
    DIR *d = opendir(COVERS_DIR);
    if (!d) return;

    struct dirent *entry;
    char filepath[1024];

    while ((entry = readdir(d)) != NULL) {
        if (entry->d_type == DT_REG) {
            snprintf(filepath, sizeof(filepath), "%s/%s", COVERS_DIR, entry->d_name);
            if (unlink(filepath) != 0) {
                perror("unlink");
            }
        }
    }

    closedir(d);
}

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) {
        printf("Not enough memory (realloc returned NULL)\n");
        return 0;
    }
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

// Sanitize ROM name by removing parentheses content, commas, dashes, extra spaces
void sanitize_rom_name(const char *input, char *output, size_t max_len) {
    int out_pos = 0;
    int skip_parens = 0;

    for (size_t i = 0; input[i] != '\0' && out_pos < max_len - 1; i++) {
        char c = input[i];

        if (c == '(') {
            skip_parens = 1;
            continue;
        }
        if (c == ')') {
            skip_parens = 0;
            continue;
        }
        if (skip_parens)
            continue;

        if (c == ',' || c == '-') {
            if (out_pos > 0 && output[out_pos - 1] != ' ')
                output[out_pos++] = ' ';
            continue;
        }

        if (isspace((unsigned char)c)) {
            if (out_pos > 0 && output[out_pos - 1] != ' ')
                output[out_pos++] = ' ';
            continue;
        }

        output[out_pos++] = c;
    }

    // Trim trailing spaces
    while (out_pos > 0 && output[out_pos - 1] == ' ')
        out_pos--;

    output[out_pos] = '\0';
}

char *get_oauth_token() {
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    struct MemoryStruct chunk = {0};
    chunk.memory = malloc(1);
    chunk.size = 0;

    char postfields[512];
    snprintf(postfields, sizeof(postfields),
             "client_id=%s&client_secret=%s&grant_type=client_credentials",
             CLIENT_ID, CLIENT_SECRET);

    curl_easy_setopt(curl, CURLOPT_URL, TOKEN_URL);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
        free(chunk.memory);
        curl_easy_cleanup(curl);
        return NULL;
    }
    curl_easy_cleanup(curl);

    cJSON *json = cJSON_Parse(chunk.memory);
    free(chunk.memory);
    if (!json) return NULL;

    cJSON *token = cJSON_GetObjectItem(json, "access_token");
    if (!token || !cJSON_IsString(token)) {
        cJSON_Delete(json);
        return NULL;
    }

    char *result = strdup(token->valuestring);
    cJSON_Delete(json);
    return result;
}

int download_image(const char *url, const char *filename) {
    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        curl_easy_cleanup(curl);
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    CURLcode res = curl_easy_perform(curl);
    fclose(fp);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        remove(filename);
        fprintf(stderr, "Failed to download %s: %s\n", url, curl_easy_strerror(res));
        return -1;
    }
    return 0;
}

int query_igdb_and_download_cover(const char *token, const char *rom_name, const char *cover_filename) {
    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    struct MemoryStruct chunk = {0};
    chunk.memory = malloc(1);
    chunk.size = 0;

    char query[512];
    snprintf(query, sizeof(query),
             "fields name,cover.image_id;"
             "search \"%s\";"
             "limit 1;",
             rom_name);

    struct curl_slist *headers = NULL;
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", token);
    headers = curl_slist_append(headers, "Client-ID: " CLIENT_ID);
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: text/plain");

    curl_easy_setopt(curl, CURLOPT_URL, IGDB_API_URL);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, query);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "IGDB query failed for '%s': %s\n", rom_name, curl_easy_strerror(res));
        free(chunk.memory);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return -1;
    }

    cJSON *json = cJSON_Parse(chunk.memory);
    free(chunk.memory);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (!json) {
        fprintf(stderr, "Failed to parse IGDB JSON for '%s'\n", rom_name);
        return -1;
    }

    if (!cJSON_IsArray(json) || cJSON_GetArraySize(json) == 0) {
        printf("No game found for: %s\n", rom_name);
        cJSON_Delete(json);
        return -1;
    }

    cJSON *game = cJSON_GetArrayItem(json, 0);
    cJSON *cover = cJSON_GetObjectItem(game, "cover");
    if (!cover || !cJSON_IsObject(cover)) {
        printf("No cover found for: %s\n", rom_name);
        cJSON_Delete(json);
        return -1;
    }

    cJSON *image_id = cJSON_GetObjectItem(cover, "image_id");
    if (!image_id || !cJSON_IsString(image_id)) {
        printf("No image_id found for: %s\n", rom_name);
        cJSON_Delete(json);
        return -1;
    }

    char image_url[512];
    snprintf(image_url, sizeof(image_url),
             "https://images.igdb.com/igdb/image/upload/t_cover_big/%s.jpg",
             image_id->valuestring);

    printf("Downloading cover for '%s' from %s\n", rom_name, image_url);
    int dl_res = download_image(image_url, cover_filename);

    cJSON_Delete(json);
    return dl_res;
}

void ensure_covers_dir() {
    struct stat st = {0};
    if (stat(COVERS_DIR, &st) == -1) {
        if (mkdir(COVERS_DIR, 0755) != 0) {
            perror("Failed to create covers directory");
            exit(EXIT_FAILURE);
        }
    } else {
        clear_covers_directory();
    }
}

void process_roms(const char *token, const char *dir_path) {
    DIR *dp = opendir(dir_path);
    if (!dp) {
        perror("Failed to open roms directory");
        return;
    }

    struct dirent *entry;
    char path[1024];
    while ((entry = readdir(dp)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);

        if (entry->d_type == DT_DIR) {
            process_roms(token, path);
            continue;
        }

        // Remove extension from filename for searching and saving
        char base_name[256];
        strncpy(base_name, entry->d_name, sizeof(base_name));
        base_name[sizeof(base_name)-1] = '\0';
        char *dot = strrchr(base_name, '.');
        if (dot) *dot = '\0';

        // Sanitize the base_name for better search
        char clean_name[256];
        sanitize_rom_name(base_name, clean_name, sizeof(clean_name));

        // Prepare cover filename based on base_name (without extension)
        char cover_path[1024];
        snprintf(cover_path, sizeof(cover_path), "%s/%s.jpg", COVERS_DIR, base_name);

        // Skip if cover already exists
        if (access(cover_path, F_OK) == 0) {
            printf("Cover already exists: %s\n", cover_path);
            continue;
        }

        printf("Searching cover for '%s'...\n", clean_name);
        if (query_igdb_and_download_cover(token, clean_name, cover_path) == 0) {
            printf("Downloaded cover: %s\n", cover_path);
        } else {
            printf("Failed to find/download cover for: %s\n", clean_name);
        }
    }

    closedir(dp);
}

int main(void) {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    ensure_covers_dir();

    char *token = get_oauth_token();
    if (!token) {
        fprintf(stderr, "Failed to get OAuth token\n");
        return 1;
    }

    printf("OAuth token obtained: %s\n", token);

    process_roms(token, ROMS_DIR);

    free(token);
    curl_global_cleanup();
    return 0;
}
