#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <libevdev-1.0/libevdev/libevdev.h>
#include <linux/input-event-codes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#define MAXEVENTS 5

int main(int argc, char *argv[]) {
    struct epoll_event events[MAXEVENTS];
    struct sockaddr_in server_addr;
    struct sockaddr client_addr;

    if (argc <= 1) {
        printf("Usage: %s <input event files>", argv[0]);
        return 1;
    }

    int inputFiles[32] = {0};
    int numFiles = argc - 1;

    for (int i = 1; i < argc; i++) {
        inputFiles[i - 1] = open(argv[i], O_RDONLY | O_NONBLOCK);
        if (inputFiles[i - 1] < 0) {
            perror(argv[i]);
            abort();
        }
    }

    int efd = epoll_create(2);
    if (efd == -1) {
        perror("epoll_create");
        abort();
    }

    for (int i = 0; i < numFiles; i++) {
        struct epoll_event epollin_event;
        epollin_event.data.fd = inputFiles[i];
        epollin_event.events = EPOLLIN;
        int ms = epoll_ctl(efd, EPOLL_CTL_ADD, inputFiles[i], &epollin_event);
        if (ms == -1) {
            perror("epoll_ctl");
            abort();
        }
    }

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("Can't open socket");
        abort();
    }

    int optval = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(4200);
    server_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    if (bind(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        perror("Couldn't bind the port");
        abort();
    }

    if (listen(sock_fd, 5) != 0) {
        perror("Couldn't start listening");
        abort();
    }

    uint32_t len = sizeof(client_addr);
    int conn_fd = accept(sock_fd, &client_addr, &len);
    if (conn_fd < 0) {
        perror("Failed to accept connection");
        abort();
    }
    printf("Client connected\n");

    uint16_t typeMax[] = {SYN_MAX, KEY_MAX, REL_MAX, ABS_MAX, MSC_MAX, SW_MAX, LED_MAX, SND_MAX, REP_MAX, FF_MAX};

    write(conn_fd, &numFiles, sizeof(numFiles));

    for (int i = 0; i < numFiles; i++) {
        uint8_t evBits[EV_MAX] = {0};
        if (ioctl(inputFiles[i], EVIOCGBIT(0, EV_MAX), evBits) < 0) {
            perror("Failed to get event bits");
            abort();
        }

        write(conn_fd, evBits, sizeof(evBits));
        for (size_t bit = 1; bit < EV_MAX; bit++) {
            size_t bytePos = (bit - (bit % 8)) / 8;
            // EV_REP would need special handling
            if ((bit < EV_MSC) && (evBits[bytePos] & (1 << (bit % 8)))) {
                uint8_t typeBits[typeMax[bit]];
                memset(typeBits, 0, sizeof(typeBits));
                if (ioctl(inputFiles[i], EVIOCGBIT(bit, sizeof(typeBits)), typeBits) < 0) {
                    perror("Failed to get type bits");
                    abort();
                }
                write(conn_fd, typeBits, sizeof(typeBits));
            }
        }
    }

    bool shift_pressed = false;
    bool sending = true;
    for (int i = 0; i < numFiles; i++) {
        ioctl(inputFiles[i], EVIOCGRAB, sending);
    }
    bool running = true;
    do {
        int n = epoll_wait(efd, events, MAXEVENTS, -1);
        for (int i = 0; i < n; i++) {
            struct input_event key_press = {};
            if (read(events[i].data.fd, &key_press, sizeof(key_press)) != -1) {
                if (key_press.type == EV_KEY) {
                    if (key_press.code == KEY_LEFTSHIFT) {
                        shift_pressed = key_press.value != 0;
                    } else if ((key_press.code == KEY_ESC) && (key_press.value == 1) && shift_pressed) {
                        sending = !sending;
                        for (int i = 0; i < numFiles; i++) {
                            ioctl(inputFiles[i], EVIOCGRAB, sending);
                        }
                    }
                }
                if (sending) {
                    int error = 0;
                    socklen_t len = sizeof(error);
                    getsockopt(conn_fd, SOL_SOCKET, SO_ERROR, &error, &len);
                    if (error != 0) {
                        printf("socket error: %s\n", strerror(error));
                        running = false;
                        break;
                    }
                    write(conn_fd, &key_press, sizeof(key_press));
                }
            }
        }
    } while (running);

    printf("Shutting down\n");
    close(conn_fd);

    for (int i = 0; i < numFiles; i++) {
        ioctl(inputFiles[i], EVIOCGRAB, false);
    }

    for (int i = 0; i < numFiles; i++) {
        close(inputFiles[i]);
    }
    close(efd);
    return 0;
}
