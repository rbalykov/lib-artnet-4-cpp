#pragma once

#include <arpa/inet.h>
#include <array>
#include <cstdint>
// #include <string>
// #include <vector>

namespace ArtNet {

// Constants
constexpr uint16_t ARTNET_PORT = 6454;
constexpr uint16_t ARTNET_FPS = 44;
constexpr uint16_t ARTNET_HEADER_SIZE = 12;
constexpr uint16_t ARTNET_MAX_DMX_SIZE = 512;

// Op Codes (from spec table 1)
#pragma pack(push, 1) // Disable padding.
enum class OpCode : uint16_t {
  OpPoll = 0x2000,
  OpPollReply = 0x2100,
  OpDiagData = 0x2300,
  OpCommand = 0x2400,
  OpDataRequest = 0x2700,
  OpDataReply = 0x2800,
  OpDmx = 0x5000,
  OpNzs = 0x5100,
  OpSync = 0x5200,
  OpAddress = 0x6000,
  OpInput = 0x7000,
  OpTodRequest = 0x8000,
  OpTodData = 0x8100,
  OpTodControl = 0x8200,
  OpRdm = 0x8300,
  OpRdmSub = 0x8400,
  // Add more opcodes as needed (but not used in our core implementation)
  // OpVideoSetup = 0xa010,
  // OpVideoPalette = 0xa020,
  // OpVideoData = 0xa040,
  // OpMacMaster = 0xf000,
  // OpMacSlave = 0xf100,
  // OpFirmwareMaster = 0xf200,
  // OpFirmwareReply = 0xf300,
  // OpFileTnMaster = 0xf400,
  // OpFileFnMaster = 0xf500,
  // OpFileFnReply = 0xf600,
  // OplpProg = 0xf800,
  // OplpProgReply = 0xf900,
  // OpMedia = 0x9000,
  // OpMediaPatch = 0x9100,
  // OpMediaControl = 0x9200,
  // OpMediaContrlReply = 0x9300,
  // OpTimeCode = 0x9700,
  // OpTimeSync = 0x9800,
  // OpTrigger = 0x9900,
  // OpDirectory = 0x9a00,
  // OpDirectoryReply = 0x9b00
};
#pragma pack(pop) // Restore default alignment.

// Common Header
#pragma pack(push, 1) // Disable padding.
struct ArtHeader {
  std::array<uint8_t, 8> id; // "Art-Net\0"
  uint16_t opcode;           // OpDmx = 0x5000, low byte first
  // uint16_t version;          // Protocol version, high byte first

  explicit ArtHeader(OpCode code) {
    // Initialize with "Art-Net\0"
    const char *artnet_id = "Art-Net";
    std::memset(id.data(), 0, id.size());
    std::memcpy(id.data(), artnet_id, std::strlen(artnet_id));

    // Set initial values
    setOpcode(code);
    // setVersion(14); // Art-Net protocol version 14
  }

  void setOpcode(OpCode code) {
    // OpCode must be little-endian per spec
    opcode = static_cast<uint16_t>(code); // Already in little-endian on x86/ARM
  }

  // void setVersion(uint16_t versionNumber) {
  //   // Version must be big-endian per spec
  //   version = htons(versionNumber);
  // }
};
#pragma pack(pop) // Restore default alignment.

// ArtPoll Packet (from spec section 6.2)
#pragma pack(push, 1) // Disable padding.
struct ArtPollPacket {
  ArtHeader header;
  uint8_t flags;
  uint8_t diagPriority;
  uint8_t targetPortAddressTopHi;
  uint8_t targetPortAddressTopLo;
  uint8_t targetPortAddressBottomHi;
  uint8_t targetPortAddressBottomLo;
  uint8_t estaManHi;
  uint8_t estaManLo;
  uint8_t oemHi;
  uint8_t oemLo;

  std::array<uint8_t, 4> versionInfo;

  ArtPollPacket()
      : header(OpCode::OpPoll), flags(0), diagPriority(0), targetPortAddressTopHi(0), targetPortAddressTopLo(0),
        targetPortAddressBottomHi(0), targetPortAddressBottomLo(0), estaManHi(0), estaManLo(0), oemHi(0), oemLo(0),
        versionInfo{0, 0, 0, 0} {}
};
#pragma pack(pop) // Restore default alignment.

// ArtPollReply Packet (from spec section 6.3)
#pragma pack(push, 1) // Ensure proper packing
struct ArtPollReplyPacket {
  ArtHeader header;
  uint8_t ip[4];
  uint16_t port;
  uint8_t versionInfo[2];
  uint8_t netSwitch;
  uint8_t subSwitch;
  uint16_t oem;
  uint8_t ubeaVersion;
  uint8_t status;
  uint16_t estaMan;
  std::array<uint8_t, 18> shortName;
  std::array<uint8_t, 64> longName;
  std::array<uint8_t, 64> nodeReport;
  uint16_t numPorts;
  std::array<uint8_t, 4> portType;
  std::array<uint8_t, 4> goodInputA;
  std::array<uint8_t, 4> goodOutputA;
  std::array<uint8_t, 4> swIn;
  std::array<uint8_t, 4> swOut;
  std::array<uint8_t, 4> acnPriority;
  std::array<uint8_t, 4> swMacro;
  std::array<uint8_t, 4> swRemote;
  uint8_t style;
  std::array<uint8_t, 6> mac;
  uint8_t bindIp[4];
  uint8_t bindIndex;
  uint8_t status2;
  std::array<uint8_t, 4> goodOutputB;
  uint8_t status3;
  std::array<uint8_t, 6> defaultResponder;
  uint16_t userHi;
  uint16_t userLo;
  uint16_t refreshRateHi;
  uint16_t refreshRateLo;
  uint8_t backgroundQueuePolicy;
  std::array<uint8_t, 10> filler; // This is field 54 (optional)

  ArtPollReplyPacket()
      : header(OpCode::OpPollReply), ip{0}, port(ARTNET_PORT), versionInfo{0, 0}, netSwitch(0), subSwitch(0), oem(0), ubeaVersion(0),
        status(0), estaMan(0), numPorts(0), style(0), mac{0}, bindIp{0}, bindIndex(0), status2(0), goodOutputB{0}, status3(0),
        defaultResponder{0}, userHi(0), userLo(0), refreshRateHi(0), refreshRateLo(0), backgroundQueuePolicy(0), filler{0} {
    shortName.fill(0);
    longName.fill(0);
    nodeReport.fill(0);
    portType.fill(0);
    goodOutputA.fill(0);
    goodInputA.fill(0);
    swIn.fill(0);
    swOut.fill(0);
    acnPriority.fill(0);
    swMacro.fill(0);
    swRemote.fill(0);
  }
};
#pragma pack(pop) // Restore default alignment.
//
// ArtDmx Packet (from spec section 7.2)
#pragma pack(push, 1) // Ensure proper packing
struct ArtDmxPacket {
  ArtHeader header;
  uint16_t version;  // Protocol version, high byte first
  uint8_t sequence;  // DMX sequence number
  uint8_t physical;  // Physical port
  uint16_t universe; // Universe (network byte order)
  uint16_t length;   // Data length (network byte order)
  uint8_t data[512]; // DMX data (maximum 512 bytes)

  // Constructor to initialize the fields
  explicit ArtDmxPacket() : header(OpCode::OpDmx) {
    version = htons(14); // Art-Net protocol version 14

    sequence = 0;
    physical = 0;
    universe = 0;
    length = 0;

    std::memset(data, 0, sizeof(data)); // Initialize data to zero
  }
};
#pragma pack(pop) // Restore default packing (if not already present)

// More packet definitions will follow, here is an example:
#pragma pack(push, 1) // Ensure proper packing
struct ArtTodDataPacket {
  ArtHeader header;
  uint8_t rdmVer;
  uint8_t port;
  uint8_t spare1;
  uint8_t spare2;
  uint8_t spare3;
  uint8_t spare4;
  uint8_t spare5;
  uint8_t spare6;
  uint8_t net;
  uint8_t command;
  uint8_t addCount;
  std::array<uint8_t, 32> address;
  uint16_t uidTotalHi;
  uint16_t uidTotalLo;
  uint8_t blockCount;
  uint8_t uidCount;
  std::array<uint8_t, 48 * 32> tod;

  ArtTodDataPacket();
};
#pragma pack(pop) // Restore default packing (if not already present)
} // namespace ArtNet
