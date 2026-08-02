#include "server.h"
#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/evp.h>

#define GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WEBSOCK_KEY_LENGTH 19

// ============ WebSocket 帧头结构（修正版）============
struct _nty_ophdr {
    unsigned char fin:1;
    unsigned char rsv1:1;
    unsigned char rsv2:1;
    unsigned char rsv3:1;
    unsigned char opcode:4;
    unsigned char mask:1;
    unsigned char payload_length:7;
} __attribute__ ((packed));

struct _nty_websocket_head_126 {
    unsigned short payload_length;
    char mask_key[4];
    unsigned char data[8];
} __attribute__ ((packed));

struct _nty_websocket_head_127 {
    unsigned long long payload_length;
    char mask_key[4];
    unsigned char data[8];
} __attribute__ ((packed));

typedef struct _nty_websocket_head_127 nty_websocket_head_127;
typedef struct _nty_websocket_head_126 nty_websocket_head_126;
typedef struct _nty_ophdr nty_ophdr;

// ============ Base64 编码 ============
int base64_encode(char *in_str, int in_len, char *out_str) {    
    BIO *b64, *bio;    
    BUF_MEM *bptr = NULL;    
    size_t size = 0;    

    if (in_str == NULL || out_str == NULL)        
        return -1;    

    b64 = BIO_new(BIO_f_base64());    
    bio = BIO_new(BIO_s_mem());    
    bio = BIO_push(b64, bio);
    
    // 不添加换行符
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    
    BIO_write(bio, in_str, in_len);    
    BIO_flush(bio);    

    BIO_get_mem_ptr(bio, &bptr);    
    memcpy(out_str, bptr->data, bptr->length);    
    out_str[bptr->length] = '\0';    
    size = bptr->length;    

    BIO_free_all(bio);    
    return size;
}

// ============ 读取一行 ============
int readline(char* allbuf, int level, char* linebuf) {    
    int len = strlen(allbuf);    

    for (; level < len; ++level) {        
        if (allbuf[level] == '\r' && allbuf[level+1] == '\n') {            
            return level+2;        
        } else {            
            *(linebuf++) = allbuf[level];        
        }    
    }    

    return -1;
}

// ============ 掩码解码 ============
void demask(char *data, int len, char *mask) {    
    int i;    
    for (i = 0; i < len; i++) {        
        *(data+i) ^= *(mask+(i%4));
    }
}

// ============ 解码数据帧 ============
char* decode_packet(unsigned char *stream, char *mask, int length, int *ret) {
    nty_ophdr *hdr = (nty_ophdr*)stream;
    unsigned char *data = stream + sizeof(nty_ophdr);
    int size = 0;
    int start = 0;
    int i = 0;

    // 检查是否有掩码
    if (hdr->mask) {
        // 获取 payload 长度
        unsigned char payload_len = hdr->payload_length;
        
        if (payload_len == 126) {
            nty_websocket_head_126 *hdr126 = (nty_websocket_head_126*)data;
            size = ntohs(hdr126->payload_length);
            for (i = 0; i < 4; i++) {
                mask[i] = hdr126->mask_key[i];
            }
            start = sizeof(nty_ophdr) + sizeof(nty_websocket_head_126) - 8;
        } else if (payload_len == 127) {
            nty_websocket_head_127 *hdr127 = (nty_websocket_head_127*)data;
            // 简化处理，只取低32位
            size = (unsigned int)hdr127->payload_length;
            for (i = 0; i < 4; i++) {
                mask[i] = hdr127->mask_key[i];
            }
            start = sizeof(nty_ophdr) + sizeof(nty_websocket_head_127) - 8;
        } else {
            size = payload_len;
            // mask_key 在 data 的前4字节
            for (i = 0; i < 4; i++) {
                mask[i] = data[i];
            }
            start = sizeof(nty_ophdr) + 4;
        }
    } else {
        // 没有掩码（服务器发送的数据）
        size = hdr->payload_length;
        start = sizeof(nty_ophdr);
    }

    *ret = size;
    
    // 解码数据（如果有掩码）
    if (hdr->mask && size > 0) {
        demask((char*)(stream + start), size, mask);
    }

    return (char*)(stream + start);
}

// ============ 编码数据帧（服务器发送，不需要掩码）============
int encode_packet(char *buffer, char *mask, char *stream, int length) {
    nty_ophdr head = {0};
    // 设置 FIN 和 opcode
    head.fin = 1;
    head.opcode = 1;  // 文本帧
    head.mask = 0;    // 服务器发送不需要掩码
    
    int size = 0;

    if (length < 126) {
        head.payload_length = length;
        memcpy(buffer, &head, sizeof(nty_ophdr));
        size = sizeof(nty_ophdr);
    } else if (length < 0xffff) {
        head.payload_length = 126;
        memcpy(buffer, &head, sizeof(nty_ophdr));
        
        nty_websocket_head_126 hdr = {0};
        hdr.payload_length = htons(length);
        // 服务器不需要 mask_key
        memcpy(buffer + sizeof(nty_ophdr), &hdr, sizeof(nty_websocket_head_126) - 8);
        size = sizeof(nty_ophdr) + sizeof(nty_websocket_head_126) - 8;
    } else {
        head.payload_length = 127;
        memcpy(buffer, &head, sizeof(nty_ophdr));
        
        nty_websocket_head_127 hdr = {0};
        hdr.payload_length = length;
        // 服务器不需要 mask_key
        memcpy(buffer + sizeof(nty_ophdr), &hdr, sizeof(nty_websocket_head_127) - 8);
        size = sizeof(nty_ophdr) + sizeof(nty_websocket_head_127) - 8;
    }

    // 复制数据体
    memcpy(buffer + size, stream, length);
    return size + length;
}

// ============ WebSocket 握手 ============
int handshark(struct conn *c) {
    char linebuf[1024] = {0};
    int idx = 0;
    char sec_data[128] = {0};
    char sec_accept[64] = {0};

    do {
        memset(linebuf, 0, 1024);
        idx = readline(c->rbuffer, idx, linebuf);

        if (strstr(linebuf, "Sec-WebSocket-Key")) {
            // linebuf: Sec-WebSocket-Key: QWz1vB/77j8J8JcT/qtiLQ==
            // 找到 key 的起始位置
            char *key_start = strstr(linebuf, "Sec-WebSocket-Key: ");
            if (!key_start) {
                return -1;
            }
            key_start += 19; // 跳过 "Sec-WebSocket-Key: "
            
            // 构建拼接字符串
            char combined[256] = {0};
            strcpy(combined, key_start);
            strcat(combined, GUID);
            
            // SHA1
            SHA1((unsigned char*)combined, strlen(combined), (unsigned char*)sec_data);
            
            // Base64 编码
            base64_encode(sec_data, 20, sec_accept);

            memset(c->wbuffer, 0, BUFFER_LENGTH);
            c->wlength = sprintf(c->wbuffer, 
                "HTTP/1.1 101 Switching Protocols\r\n"
                "Upgrade: websocket\r\n"
                "Connection: Upgrade\r\n"
                "Sec-WebSocket-Accept: %s\r\n\r\n", 
                sec_accept);

            printf("WebSocket 响应:\n%s\n", c->wbuffer);
            break;
        }

    } while ((c->rbuffer[idx] != '\r' || c->rbuffer[idx+1] != '\n') && idx != -1);

    return 0;
}

// ============ WebSocket 请求处理 ============
int ws_request(struct conn *c) {
    printf("WebSocket 请求: %s\n", c->rbuffer);

    if (c->status == 0) {
        // 第一次连接，执行握手
        handshark(c);
        c->status = 1;  // 握手完成
    } else if (c->status == 1) {
        // 数据通信
        char mask[4] = {0};
        int ret = 0;

        c->payload = decode_packet((unsigned char*)c->rbuffer, 
                                   c->mask, 
                                   c->rlength, 
                                   &ret);

        if (ret > 0) {
            printf("收到数据: %s , 长度: %d\n", c->payload, ret);
            
            // 回显数据
            c->wlength = ret;
            c->status = 2;  // 准备发送数据
        }
    }

    return 0;
}

// ============ WebSocket 响应处理 ============
int ws_response(struct conn *c) {
    if (c->status == 2) {
        // 编码并发送数据
        c->wlength = encode_packet(c->wbuffer, 
                                   c->mask, 
                                   c->payload, 
                                   c->wlength);
        c->status = 1;  // 回到数据接收状态
    }

    return 0;
}