#include <sys/param.h>
#include <inttypes.h>

#include "esp_log.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

#define DNS_PORT (53)
#define DNS_MAX_LEN (512)

#define OPCODE_MASK (0x7800)
#define QR_FLAG (1 << 7)
#define QD_TYPE_A (0x0001)
#define ANS_TTL_SEC (300)

static const char* TAG = "dns_server";

typedef struct __attribute__((__packed__))
{
    uint16_t id;
    uint16_t flags;
    uint16_t qd_count;
    uint16_t an_count;
    [[maybe_unused]] uint16_t ns_count;
    [[maybe_unused]] uint16_t ar_count;
} dns_header_t;

typedef struct
{
    uint16_t type;
    uint16_t class;
} dns_question_t;

typedef struct __attribute__((__packed__))
{
    uint16_t ptr_offset;
    uint16_t type;
    uint16_t class;
    uint32_t ttl;
    uint16_t addr_len;
    uint32_t ip_addr;
} dns_answer_t;

// ReSharper disable once CppDFAConstantParameter
static char* parse_dns_name(char* raw_name, char* parsed_name, const size_t parsed_name_max_len)
{
    char* label = raw_name;
    char* name_itr = parsed_name;
    int name_len = 0;

    do
    {
        const int sub_name_len = *label;
        name_len += (sub_name_len + 1);
        if (name_len > parsed_name_max_len)
        {
            return nullptr;
        }

        memcpy(name_itr, label + 1, sub_name_len);
        name_itr[sub_name_len] = '.';
        name_itr += (sub_name_len + 1);
        label += sub_name_len + 1;
    }
    while (*label != 0);

    parsed_name[name_len - 1] = '\0';
    return label + 1;
}

// ReSharper disable once CppDFAConstantParameter
static int parse_dns_request(const char* req, const size_t req_len, char* dns_reply, const size_t dns_reply_max_len)
{
    if (req_len > dns_reply_max_len)
    {
        return -1;
    }

    memset(dns_reply, 0, dns_reply_max_len);
    memcpy(dns_reply, req, req_len);

    dns_header_t* header = (dns_header_t*)dns_reply;
    ESP_LOGD(TAG, "DNS query with header id: 0x%X, flags: 0x%X, qd_count: %d",
             ntohs(header->id), ntohs(header->flags), ntohs(header->qd_count));

    if ((header->flags & OPCODE_MASK) != 0)
    {
        return 0;
    }

    header->flags |= QR_FLAG;

    const uint16_t qd_count = ntohs(header->qd_count);
    header->an_count = htons(qd_count);

    const unsigned int reply_len = qd_count * sizeof(dns_answer_t) + req_len;
    if (reply_len > dns_reply_max_len)
    {
        return -1;
    }

    char* cur_ans_ptr = dns_reply + req_len;
    char* cur_qd_ptr = dns_reply + sizeof(dns_header_t);
    char name[128];

    for (int i = 0; i < qd_count; i++)
    {
        char* name_end_ptr = parse_dns_name(cur_qd_ptr, name, sizeof(name));
        if (name_end_ptr == NULL)
        {
            ESP_LOGE(TAG, "Failed to parse DNS question: %s", cur_qd_ptr);
            return -1;
        }

        const dns_question_t* question = (dns_question_t*)name_end_ptr;
        const uint16_t qd_type = ntohs(question->type);
        const uint16_t qd_class = ntohs(question->class);

        ESP_LOGD(TAG, "Received type: %d | Class: %d | Question for: %s", qd_type, qd_class, name);

        if (qd_type == QD_TYPE_A)
        {
            dns_answer_t* answer = (dns_answer_t*)cur_ans_ptr;

            answer->ptr_offset = htons(0xC000 | (cur_qd_ptr - dns_reply));
            answer->type = htons(qd_type);
            answer->class = htons(qd_class);
            answer->ttl = htonl(ANS_TTL_SEC);

            esp_netif_ip_info_t ip_info;
            esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);
            ESP_LOGD(TAG, "Answer with PTR offset: 0x%" PRIX16 " and IP 0x%" PRIX32, ntohs(answer->ptr_offset),
                     ip_info.ip.addr);

            answer->addr_len = htons(sizeof(ip_info.ip.addr));
            answer->ip_addr = ip_info.ip.addr;
        }
    }
    return (int)reply_len;
}

void dns_server_task([[maybe_unused]]void* )
{
    char rx_buffer[DNS_MAX_LEN];
    char addr_str[128];

    while (1)
    {
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(DNS_PORT);
        constexpr int addr_family = AF_INET;
        constexpr int ip_protocol = IPPROTO_IP;
        inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);

        const int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0)
        {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket created");

        int err = bind(sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        if (err < 0)
        {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
            close(sock);
            break;
        }
        ESP_LOGI(TAG, "Socket bound, port %d", DNS_PORT);

        while (1)
        {
            ESP_LOGI(TAG, "Waiting for data");
            struct sockaddr_in source_addr;
            socklen_t socklen = sizeof(source_addr);
            const int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr*)&source_addr,
                                     &socklen);

            if (len < 0)
            {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                close(sock);
                break;
            }
            inet_ntoa_r(source_addr.sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);

            rx_buffer[len] = 0;

            char reply[DNS_MAX_LEN];
            const int reply_len = parse_dns_request(rx_buffer, len, reply, DNS_MAX_LEN);

            ESP_LOGI(TAG, "Received %d bytes from %s | DNS reply with len: %d", len, addr_str, reply_len);
            if (reply_len <= 0)
            {
                ESP_LOGE(TAG, "Failed to prepare a DNS reply");
            }
            else
            {
                err = sendto(sock, reply, reply_len, 0, (struct sockaddr*)&source_addr, sizeof(source_addr));
                if (err < 0)
                {
                    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                    break;
                }
            }
        }

        ESP_LOGE(TAG, "Shutting down socket");
        shutdown(sock, 0);
        close(sock);
    }
    vTaskDelete(nullptr);
}

void start_dns_server(void)
{
    xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 5, nullptr);
}
