// The listening itself, on a Mac: CoreBluetooth hears every Bluetooth LE advertisement nearby
// and each one becomes a call in the Room. Compiled with ARC (see CMakeLists.txt).
#include "Radio.h"
#include <atomic>
#import <CoreBluetooth/CoreBluetooth.h>

@interface ScqRadioListener : NSObject <CBCentralManagerDelegate>
{
@public
    radio::Room* room;
    std::atomic<int> state;
}
@end

@implementation ScqRadioListener

- (void) centralManagerDidUpdateState: (CBCentralManager*) central
{
    switch (central.state)
    {
        case CBManagerStatePoweredOn:
            // every advertisement, not only the first from each device: the count needs to
            // know who is still here, and the pulses are the advertisements themselves
            [central scanForPeripheralsWithServices: nil
                                            options: @{ CBCentralManagerScanOptionAllowDuplicatesKey: @YES }];
            state = (int) radio::State::listening;
            break;
        case CBManagerStatePoweredOff:   state = (int) radio::State::bluetoothOff; break;
        case CBManagerStateUnauthorized: state = (int) radio::State::notAllowed; break;
        case CBManagerStateUnsupported:  state = (int) radio::State::noRadio; break;
        default:                         state = (int) radio::State::asking; break;   // unknown, resetting
    }
    if (state != (int) radio::State::listening && room != nullptr) room->clear();
}

- (void) centralManager: (CBCentralManager*) central
  didDiscoverPeripheral: (CBPeripheral*) peripheral
      advertisementData: (NSDictionary<NSString*, id>*) advertisementData
                   RSSI: (NSNumber*) RSSI
{
    juce::ignoreUnused (central, advertisementData);
    if (room == nullptr) return;
    // the identifier macOS gives a device is already a stand-in for its address; only a
    // number made from it is kept
    uuid_t u;
    [peripheral.identifier getUUIDBytes: u];
    juce::uint64 a = 0, b = 0;
    memcpy (&a, u, 8);
    memcpy (&b, u + 8, 8);
    room->heard (a ^ (b * 0x9E3779B97F4A7C15ull), RSSI.intValue, juce::Time::getMillisecondCounterHiRes() * 0.001);
}

@end

namespace radio
{

struct Radio::Native
{
    ScqRadioListener* listener = nil;
    CBCentralManager* central = nil;
    dispatch_queue_t queue = nullptr;
};

Radio::Radio() : native (std::make_unique<Native>()) {}

Radio::~Radio() { setWanted (false); }

void Radio::setWanted (bool w)
{
    if (w == want) return;
    want = w;
    if (w)
    {
        native->listener = [[ScqRadioListener alloc] init];
        native->listener->room = &room;
        native->listener->state = (int) State::asking;
        native->queue = dispatch_queue_create ("com.sleepcousonique.radio", DISPATCH_QUEUE_SERIAL);
        // (no system alert about Bluetooth being off: the status line says so)
        native->central = [[CBCentralManager alloc] initWithDelegate: native->listener
                                                               queue: native->queue
                                                             options: @{ CBCentralManagerOptionShowPowerAlertKey: @NO }];
    }
    else
    {
        if (native->central != nil)
        {
            if (native->central.state == CBManagerStatePoweredOn) [native->central stopScan];
            native->central.delegate = nil;
        }
        if (native->queue != nullptr) dispatch_sync (native->queue, ^{});   // let a call in flight finish
        if (native->listener != nil) native->listener->room = nullptr;
        native->central = nil;
        native->listener = nil;
        native->queue = nullptr;
        room.clear();
    }
}

State Radio::state() const
{
    if (! want || native->listener == nil) return State::off;
    return (State) native->listener->state.load();
}

}
