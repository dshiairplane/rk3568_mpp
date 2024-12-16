CC = /opt/atk-dlrk356x-toolchain/bin/aarch64-buildroot-linux-gnu-gcc
CFLAGS = -Wall -O2
CFLAGS += -I/home/dshiairplane/rk3568/sdk/linux/rk3568_linux_sdk/buildroot/output/rockchip_rk3568/target/usr/include
CFLAGS += -I/home/dshiairplane/rk3568/sdk/linux/rk3568_linux_sdk/buildroot/output/rockchip_rk3568/target/usr/include/rockchip
LDFLAGS = -L/home/dshiairplane/rk3568/sdk/linux/rk3568_linux_sdk/buildroot/output/rockchip_rk3568/target/usr/lib
LDLIBS = -lrockchip_vpu -lrockchip_mpp

SRC = imx415.c
TARGET = IMX415

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean