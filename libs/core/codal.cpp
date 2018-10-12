#include "pxt.h"
#include <stdarg.h>

PXT_ABI(__aeabi_dadd)
PXT_ABI(__aeabi_dcmplt)
PXT_ABI(__aeabi_dcmpgt)
PXT_ABI(__aeabi_dsub)
PXT_ABI(__aeabi_ddiv)
PXT_ABI(__aeabi_dmul)

extern "C" void target_panic(int error_code)
{
    // wait for serial to flush
    wait_us(300000);
    microbit_panic(error_code);
}

extern "C" void target_reset()
{
    microbit_reset();
}

namespace pxt {

MicroBit uBit;
MicroBitEvent lastEvent;

void platform_init() {
    microbit_seed_random();
    seedRandom(microbit_random(0x7fffffff));
}

void platform_init();
void usb_init();

struct FreeList {
    FreeList *next;
};

void dispatchBackground(MicroBitEvent e, void* action) {
    lastEvent = e;
    auto value = fromInt(e.value);
    runAction1((Action)action, value);
}

void dispatchForeground(MicroBitEvent e, void* action) {
    lastEvent = e;
    auto value = fromInt(e.value);
    runAction1((Action)action, value);
}

void deleteListener(MicroBitListener *l) {
    if (l->cb_param == (void (*)(MicroBitEvent, void*))dispatchBackground || 
        l->cb_param == (void (*)(MicroBitEvent, void*))dispatchForeground)
        decr((Action)(l->cb_arg));
}

static void initCodal() {

    uBit.init();

    uBit.messageBus.setListenerDeletionCallback(deleteListener);

    // repeat error 4 times and restart as needed
    microbit_panic_timeout(4);
}

void dumpDmesg() {}

// ---------------------------------------------------------------------------
// An adapter for the API expected by the run-time.
// ---------------------------------------------------------------------------

static bool backgroundHandlerFlag = false;
void setBackgroundHandlerFlag() {
    backgroundHandlerFlag = true;
}

void registerWithDal(int id, int event, Action a) {
    if (!backgroundHandlerFlag) {
        uBit.messageBus.ignore(id, event, dispatchForeground);
    }
    uBit.messageBus.listen(id, event, backgroundHandlerFlag ? dispatchBackground : dispatchForeground, a);
    backgroundHandlerFlag = false;
    incr((Action)a);
}

void unregisterFromDal(Action a) { 
    uBit.messageBus.ignore(MICROBIT_EVT_ANY, MICROBIT_EVT_ANY, dispatchBackground, (void*) a);
}

void fiberDone(void *a) {
    decr((Action)a);
    release_fiber();
}

void releaseFiber() {
    release_fiber();
}

void sleep_ms(unsigned ms) {
    fiber_sleep(ms);
}

void sleep_us(uint64_t us) {
    wait_us(us);
}

void forever_stub(void *a) {
    while (true) {
        runAction0((Action)a);
        fiber_sleep(20);
    }
}

void runForever(Action a) {
    if (a != 0) {
        incr(a);
        create_fiber(forever_stub, (void *)a);
    }
}

void runInParallel(Action a) {
    if (a != 0) {
        incr(a);
        create_fiber((void (*)(void *))runAction0, (void *)a, fiberDone);
    }
}

void waitForEvent(int id, int event) {
    fiber_wait_for_event(id, event);
}

void initRuntime() {
    initCodal();
    platform_init();
}

//%
unsigned afterProgramPage() {
    unsigned ptr = (unsigned)&bytecode[0];
    ptr += programSize();
    ptr = (ptr + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1);
    return ptr;
}


int current_time_ms() {
    return system_timer_current_time();
}

static void logwriten(const char *msg, int l)
{
    uBit.serial.send((uint8_t*)msg, l);
}

static void logwrite(const char *msg)
{
    logwriten(msg, strlen(msg));
}


static void writeNum(char *buf, uint32_t n, bool full)
{
    int i = 0;
    int sh = 28;
    while (sh >= 0)
    {
        int d = (n >> sh) & 0xf;
        if (full || d || sh == 0 || i)
        {
            buf[i++] = d > 9 ? 'A' + d - 10 : '0' + d;
        }
        sh -= 4;
    }
    buf[i] = 0;
}

static void logwritenum(uint32_t n, bool full, bool hex)
{
    char buff[20];

    if (hex)
    {
        writeNum(buff, n, full);
        logwrite("0x");
    }
    else
    {
        itoa(n, buff);
    }

    logwrite(buff);
}

void vdebuglog(const char *format, va_list ap)
{
    const char *end = format;

    while (*end)
    {
        if (*end++ == '%')
        {
            logwriten(format, end - format - 1);
            uint32_t val = va_arg(ap, uint32_t);
            switch (*end++)
            {
            case 'c':
                logwriten((const char *)&val, 1);
                break;
            case 'd':
                logwritenum(val, false, false);
                break;
            case 'x':
                logwritenum(val, false, true);
                break;
            case 'p':
            case 'X':
                logwritenum(val, true, true);
                break;
            case 's':
                logwrite((char *)(void *)val);
                break;
            case '%':
                logwrite("%");
                break;
            default:
                logwrite("???");
                break;
            }
            format = end;
        }
    }
    logwriten(format, end - format);
    logwrite("\n");
}

void debuglog(const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    vdebuglog(format, arg);
    va_end(arg);
}




} // namespace pxt



