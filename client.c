#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/uinput.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <server-ip>\n", argv[0]);
        return 1;
    }

    struct sockaddr_in server_addr;

    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

    if (fd < 0) {
        perror("open /dev/uinput");
        abort();
    }

    struct uinput_setup setup = {.name = "Net-KM",
                                 .id = {
                                     .bustype = BUS_USB,
                                     .vendor = 0x0,
                                     .product = 0x0,
                                     .version = 1,
                                 }};

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("Can't open socket");
        abort();
    }

    int optval = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(4200);
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
    uint32_t len = sizeof(server_addr);
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to connect to the server");
        abort();
    }

    uint16_t typeMax[] = {SYN_MAX, KEY_MAX, REL_MAX, ABS_MAX, MSC_MAX, SW_MAX, LED_MAX, SND_MAX, REP_MAX, FF_MAX};
    uint8_t evBits[EV_MAX] = {0};
    int filesLength = 0;

    if (read(sock_fd, &filesLength, sizeof(filesLength)) < 0) {
        perror("Failed number of devices");
        abort();
    }

    for (int i = 0; i < filesLength; i++) {
        memset(evBits, 0, sizeof(evBits));
        if (read(sock_fd, evBits, sizeof(evBits)) < 0) {
            perror("Failed to get event bits");
            abort();
        }

        for (uint64_t evBit = 1; evBit < EV_MAX; evBit++) {
            size_t evBytePos = (evBit - (evBit % 8)) / 8;
            if ((evBit < EV_MSC) && (evBits[evBytePos] & (1 << (evBit % 8)))) {
                if (ioctl(fd, UI_SET_EVBIT, evBit) < 0) {
                    perror("Failed to set event bit");
                    abort();
                }
                uint8_t typeBits[typeMax[evBit]];
                memset(typeBits, 0, sizeof(typeBits));
                if (read(sock_fd, typeBits, sizeof(typeBits)) < 0) {
                    perror("Failed to get type bits");
                    abort();
                }

                for (size_t bit = 0; bit < sizeof(typeBits); bit++) {
                    size_t bytePos = (bit - (bit % 8)) / 8;
                    if (typeBits[bytePos] & (1 << (bit % 8))) {
                        switch (evBit) {
                            case EV_KEY: {
                                ioctl(fd, UI_SET_KEYBIT, bit);
                                break;
                            }
                            case EV_REL: {
                                ioctl(fd, UI_SET_RELBIT, bit);
                                break;
                            }
                            case EV_ABS: {
                                ioctl(fd, UI_SET_ABSBIT, bit);
                                break;
                            }
                            case EV_MSC: {
                                ioctl(fd, UI_SET_MSCBIT, bit);
                                break;
                            }
                            case EV_SW: {
                                ioctl(fd, UI_SET_SWBIT, bit);
                                break;
                            }
                            case EV_LED: {
                                ioctl(fd, UI_SET_LEDBIT, bit);
                                break;
                            }
                            case EV_SND: {
                                ioctl(fd, UI_SET_SNDBIT, bit);
                                break;
                            }
                            default:
                                break;
                        }
                    }
                }
            }
        }
    }
    if (ioctl(fd, UI_DEV_SETUP, &setup)) {
        perror("UI_DEV_SETUP");
        abort();
    }

    if (ioctl(fd, UI_DEV_CREATE)) {
        perror("UI_DEV_CREATE");
        abort();
    }

    do {
        struct input_event output_event = {0};  // output event
        struct sockaddr_in rec_server_addr = {0};
        if (0 < read(sock_fd, &output_event, sizeof(output_event))) {
            if (write(fd, &output_event, sizeof output_event) < 0) {
                perror("write");
                return 1;
            }
        }
    } while (1);

    if (ioctl(fd, UI_DEV_DESTROY)) {
        printf("UI_DEV_DESTROY");
        return 1;
    }

    close(fd);
    return 0;
}
