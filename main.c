#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>




#define BROADCAST_ADDR "255.255.255.255"
#define BROADCAST_PORT 4000
#define SendSleepTime 3

float get_temperature();

float get_temperature() {
    FILE *fp;
    char buf[128];
    char temperature[10];
    float temp;
    fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (fp == NULL) {
        printf("open file error\n");
        return -1;
    }
    fgets(buf, sizeof(buf), fp);
    strncpy(temperature, buf, 2);
    temperature[2] = '.';
    strncpy(temperature + 3, buf + 2, 3);
    temp = atof(temperature);
    fclose(fp);
    return temp;
}

void *sender() {
    //1.创建udp套接字
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("socket error");
        return (void *) -1;
    }
    //2.开启广播
    int on = 1;
    int ret = setsockopt(sock_fd, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));
    if (ret < 0) {
        perror("setsockopt error");
        goto err;
    }

    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;//地址族IPV4
    dest_addr.sin_port = htons(BROADCAST_PORT);//设置端口号
    dest_addr.sin_addr.s_addr = inet_addr(BROADCAST_ADDR);//设置广播地址
    char time_str[128] = "";
    time_t now;

    while (1) {
        char host_buffer[128];
        ret = gethostname(host_buffer, sizeof(host_buffer));
        if (ret < 0) {
            perror("get hostname error");
            goto err;
        }
        char *to_send = (char *) malloc(1024);
        time(&now);
        strftime(time_str, 128, "%Y-%m-%d %H:%M:%S", localtime(&now));

        float temp = get_temperature();
        sprintf(to_send, "{\"host\": \"%s\", \"time: \"%s\", \"from\": \"C\", \"temp\": \"%.2f\"}", host_buffer, time_str, temp);
        // 3.发送数据
        ret = sendto(sock_fd, to_send, strlen(to_send), 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr));
        free(to_send);
        if (ret < 0) {
            perror("sendto error");
            goto err;
        }
        sleep(SendSleepTime);
    }
    err:
    // 4.关闭套接字
    close(sock_fd);
    return NULL;
}

void *receiver() {

    //1.创建udp套接字
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("socket error");
        return (void *) -1;
    }
    //设置端口地址复用
    int on = 1;
    int rt = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (rt < 0) {
        perror("setsockopt error");
        goto recv_err;
    }

    //2.绑定地址
    struct sockaddr_in src_addr;
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.sin_family = AF_INET;//地址族IPV4
    src_addr.sin_port = htons(BROADCAST_PORT);//设置端口号
    src_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int ret = bind(sock_fd, (struct sockaddr *) &src_addr, sizeof(src_addr));
    if (ret < 0) {
        perror("bind error");
        goto recv_err;
    }

    while (1) {
        //3.接收数据
        char buffer[1024] = {0};
        struct sockaddr_in send_addr;
        socklen_t len = sizeof(send_addr);
        ret = recvfrom(sock_fd, buffer, sizeof(buffer), 0, (struct sockaddr *) &send_addr, &len);
        if (ret < 0) {
            perror("recvfrom error");
            goto recv_err;
        }
        char * ipString = inet_ntoa(send_addr.sin_addr);

        printf("recv: %s from %s:%d\n", buffer, ipString, send_addr.sin_port);
    }
    recv_err:
    //4.关闭套接字
    close(sock_fd);
    return NULL;
}

int main(int argc, char **argv) {

    pthread_t p_send, p_recv;
    pthread_create(&p_send, NULL, sender, NULL);
    pthread_create(&p_recv, NULL, receiver, NULL);

    pthread_join(p_send, NULL);
    pthread_join(p_recv, NULL);
    return 0;
}




