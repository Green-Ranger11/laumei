#include "procfs.h"

#include <QSysInfo>

#import <Foundation/Foundation.h>
#import <IOKit/hidsystem/IOHIDEventSystemClient.h>
#include <mach/mach.h>
#include <sys/sysctl.h>

// Apple Silicon temperature sensors are only reachable through these
// IOHIDEventSystem calls (what powermetrics-free tools such as Stats use).
extern "C" {
typedef struct __IOHIDEvent* IOHIDEventRef;
typedef struct __IOHIDServiceClient* IOHIDServiceClientRef;
IOHIDEventSystemClientRef IOHIDEventSystemClientCreate(CFAllocatorRef allocator);
int IOHIDEventSystemClientSetMatching(IOHIDEventSystemClientRef client, CFDictionaryRef match);
IOHIDEventRef IOHIDServiceClientCopyEvent(IOHIDServiceClientRef, int64_t type, int32_t, int64_t);
CFStringRef IOHIDServiceClientCopyProperty(IOHIDServiceClientRef service, CFStringRef property);
double IOHIDEventGetFloatValue(IOHIDEventRef event, int32_t field);
}

namespace {

constexpr int64_t kHIDEventTypeTemperature = 15;

QByteArray procStat() {
    natural_t count = 0;
    processor_info_array_t info = nullptr;
    mach_msg_type_number_t infoCount = 0;
    if (host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &count, &info, &infoCount) != KERN_SUCCESS)
        return {};
    unsigned long long user = 0, nice = 0, system = 0, idle = 0;
    auto* load = reinterpret_cast<processor_cpu_load_info_t>(info);
    for (natural_t i = 0; i < count; i++) {
        user += load[i].cpu_ticks[CPU_STATE_USER];
        nice += load[i].cpu_ticks[CPU_STATE_NICE];
        system += load[i].cpu_ticks[CPU_STATE_SYSTEM];
        idle += load[i].cpu_ticks[CPU_STATE_IDLE];
    }
    vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(info), infoCount * sizeof(integer_t));
    // user nice system idle iowait irq softirq steal guest guest_nice
    return QString("cpu  %1 %2 %3 %4 0 0 0 0 0 0\n").arg(user).arg(nice).arg(system).arg(idle).toUtf8();
}

QByteArray procMeminfo() {
    uint64_t total = 0;
    size_t len = sizeof(total);
    sysctlbyname("hw.memsize", &total, &len, nullptr, 0);

    vm_statistics64_data_t vm{};
    mach_msg_type_number_t n = HOST_VM_INFO64_COUNT;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vm), &n) != KERN_SUCCESS)
        return {};
    const uint64_t page = vm_kernel_page_size;
    // Activity Monitor's "Memory Used": app memory + wired + compressed.
    const uint64_t used = (uint64_t(vm.internal_page_count) - vm.purgeable_count + vm.wire_count +
                           vm.compressor_page_count) * page;
    const uint64_t available = total > used ? total - used : 0;
    return QString("MemTotal:       %1 kB\nMemFree:        %2 kB\nMemAvailable:   %3 kB\n")
        .arg(total / 1024)
        .arg(uint64_t(vm.free_count) * page / 1024)
        .arg(available / 1024)
        .toUtf8();
}

// Hottest CPU die sensor ("PMU tdie*"), in millidegrees like the kernel.
QByteArray cpuTemperature() {
    @autoreleasepool {
        IOHIDEventSystemClientRef client = IOHIDEventSystemClientCreate(kCFAllocatorDefault);
        if (!client) return {};
        // Vendor usage page 0xff00, usage 5: temperature sensors.
        NSDictionary* match = @{@"PrimaryUsagePage": @0xff00, @"PrimaryUsage": @5};
        IOHIDEventSystemClientSetMatching(client, (__bridge CFDictionaryRef)match);
        CFArrayRef services = IOHIDEventSystemClientCopyServices(client);
        double hottest = 0;
        for (CFIndex i = 0; services && i < CFArrayGetCount(services); i++) {
            auto service = (IOHIDServiceClientRef)CFArrayGetValueAtIndex(services, i);
            CFStringRef name = IOHIDServiceClientCopyProperty(service, CFSTR("Product"));
            const bool die = name && [(__bridge NSString*)name containsString:@"tdie"];
            if (name) CFRelease(name);
            if (!die) continue;
            if (IOHIDEventRef ev = IOHIDServiceClientCopyEvent(service, kHIDEventTypeTemperature, 0, 0)) {
                hottest = std::max(hottest, IOHIDEventGetFloatValue(ev, kHIDEventTypeTemperature << 16));
                CFRelease(ev);
            }
        }
        if (services) CFRelease(services);
        CFRelease(client);
        if (hottest <= 0) return {};
        return QByteArray::number(qRound64(hottest * 1000)) + '\n';
    }
}

} // namespace

namespace ProcFs {

std::optional<QByteArray> read(const QString& path) {
    if (path == "/proc/stat") return procStat();
    if (path == "/proc/meminfo") return procMeminfo();
    if (path == "/sys/class/thermal/thermal_zone0/temp") return cpuTemperature();
    if (path == "/etc/hostname") return QSysInfo::machineHostName().section('.', 0, 0).toUtf8() + '\n';
    return std::nullopt;
}

} // namespace ProcFs
