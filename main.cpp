#include "hwid.h"

int main() {
    auto HWID{ HardwareId() };

    printf(HWID.Disk.SerialNumber);

    getchar();
    return 0;
}
