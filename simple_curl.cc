#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <deque>
#include <curl/curl.h>
#include <curl/easy.h>

// definition of payload chunk
struct ChunkData {
    char *memory;
    size_t size;
};
// payload chunks with sequence number
std::map<unsigned long, ChunkData> payload;

// definition of byte array buffer
struct BufferData {
    char *memory;
    size_t size;
    size_t cap;
    size_t pos;
};

// save payload chunks to file handle
size_t save_payload(FILE* fd) {
    size_t total_written = 0;
    printf("payload size:%lu last seq:%lu\n", payload.size(), payload.end()->first);
    for (auto it = payload.begin(); it != payload.end(); it++) {
        size_t written = fwrite(it->second.memory, 1, it->second.size, fd);
        if (written != it->second.size) {
            return 0;
        }
        total_written += written;
    }
    return total_written;
}

// using deque as fifo buffer
size_t fn_write_queue(void* ptr, size_t size, size_t nmemb, void* buff_ptr) {
    size_t pack_len = size * nmemb;
    std::deque<char> *buff = (std::deque<char> *)buff_ptr;
    for (size_t len = 0; len < pack_len; len++) {
        buff->push_back(*((char*)ptr+len));
    }
    printf("buff:%lu ", buff->size());
    // check size of buffer is large than seq(32bits) + chunk size(16bits) + chunk data
    while (buff->size() > 6) {
        unsigned short len = ((unsigned short)(*buff)[4] << 8) + 
                             (unsigned char)(*buff)[5];
        if (buff->size() < 6 + len)
            break;
        unsigned long seq = ((unsigned long)(*buff)[0] << 24) + 
                            ((unsigned long)(*buff)[1] << 16) + 
                            ((unsigned long)(*buff)[2] << 8) + 
                            (unsigned char)(*buff)[3];
        printf("seq:%lu len:%u\n", seq, len);
        payload[seq].size = len;
        payload[seq].memory = (char*)malloc(len);
        for (size_t i = 0; i < len + 6; i++) {
            if (i >= 6) {
                payload[seq].memory[i - 6] = buff->front();
            }
            buff->pop_front();
        }
    }
    
    printf("bsz:%lu psz:%lu\n", buff->size(), payload.size());
    return pack_len;
}

size_t fn_write_buffer(void* ptr, size_t size, size_t nmemb, void* buff_ptr) {
    size_t pack_len = size * nmemb;
    BufferData* buff = (BufferData*)buff_ptr;
    auto chk = buff->memory;
    // resize buffer capacity for arrival packs
    if (buff->cap < buff->size + pack_len + 1) {
        chk = (char*)realloc(buff->memory, buff->size + pack_len + 1);
        buff->cap = buff->size + pack_len + 1;
    }
    if (chk == NULL) {
        printf("buffer reallocation failed\n");
        return 0;
    }

    // prevent start address changed after realloc
    buff->memory = chk;
    // assign destination to the end of buffer
    auto dst = chk + buff->size;
    memcpy(dst, ptr, pack_len);
    buff->size += pack_len;

    // check buffer size is large than seq(32bits) + chunk size(16bits) + chunk data
    while (buff->size > buff->pos + 6) {
        // both chunk length and sequence are big-endian
        unsigned short len = ((unsigned short)buff->memory[buff->pos + 4] << 8) + 
                             (unsigned char)buff->memory[buff->pos + 5];
        // check if whole payload has been filled
        if (buff->size < buff->pos + 6 + len)
            break;
        unsigned long seq = ((unsigned long)buff->memory[buff->pos] << 24) + 
                            ((unsigned long)buff->memory[buff->pos + 1] << 16) + 
                            ((unsigned long)buff->memory[buff->pos + 2] << 8) + 
                            (unsigned char)buff->memory[buff->pos + 3];
        printf("buff pos:%lu\tseq:%lu len:%u\n", buff->pos, seq, len);
        payload[seq].size = len;
        payload[seq].memory = (char*)malloc(len);
        memcpy(payload[seq].memory, chk + buff->pos + 6, len);
        buff->pos += (6 + len);
    }

    // reset working position and buffer allocation to save memory usage
    size_t reset_size = buff->size - buff->pos;
    if (buff->pos > 0 && reset_size >= 0) {
        if (reset_size > 0)
            memcpy(buff->memory, buff->memory + buff->pos, reset_size);
        buff->pos = 0;
        buff->size = reset_size;
        printf("buff pos reset, rest:%lu\n", reset_size);
    }

    return pack_len;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        // print usage
        printf("Usage: ./simple_curl <url> <file_name>\n");
        return 0;
    } else {
        printf("input url:%s\n", argv[1]);
        printf("output file:%s\n", argv[2]);
    }

    CURL *cu;
    CURLcode result;
    char err[CURL_ERROR_SIZE];
    curl_global_init(CURL_GLOBAL_ALL);
    cu = curl_easy_init();
    if (!cu) {
        printf("curl init failed\n");
        return 0;
    }

    // for fn_write_buffer
    BufferData buffer;
    buffer.memory = (char*)malloc(1);
    buffer.cap = 1;
    buffer.size = 0;
    buffer.pos = 0;
    if (!buffer.memory) {
        printf("buffer allocation failed\n");
        return 0;
    }
    // for fn_write_queue
    std::deque<char> queue;

    curl_easy_setopt(cu, CURLOPT_URL, argv[1]);
    curl_easy_setopt(cu, CURLOPT_WRITEFUNCTION, fn_write_buffer);
    curl_easy_setopt(cu, CURLOPT_WRITEDATA, &buffer);
    //curl_easy_setopt(cu, CURLOPT_WRITEFUNCTION, fn_write_queue);
    //curl_easy_setopt(cu, CURLOPT_WRITEDATA, &queue);
    curl_easy_setopt(cu, CURLOPT_ERRORBUFFER, err);
    result = curl_easy_perform(cu);
    if (result != CURLE_OK) {
        printf("curl retrieve failed code:%d err:%s\n", result, err);
    }
    curl_easy_cleanup(cu);
    
    FILE *fd = fopen(argv[2], "wb");
    if (fd) {
        size_t written = save_payload(fd);
        printf("save bytes:%lu\n", written);
        fclose(fd);
    } else {
        printf("can not open output file\n");
    }

    free(buffer.memory);
    return 0;
}